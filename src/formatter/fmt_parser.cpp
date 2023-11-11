#include "fmt_parser.h"

#include "core/types.h"
#include "core/utils.h"
#include "containers/string.h"
#include "containers/hash_table.h"
#include "containers/string_builder.h"
#include "fileio/fileio.h"
#include "serialization/json.h"

#include "fmt_variables.h"
#include "fmt_lexer.h"
#include "fmt_error.h"

namespace Fmt
{

struct ParseContext
{
    String content;
    StringBuilder& builder;

    DynamicArray<Token> tokens;
    VariableData* parent_var;
    u64 token_index;

    DynamicArray<VariableData> var_stack;
    DynamicArray<Token> op_stack;

    HashTable<String, TokenList> tokenized_files;
};

static inline ParseContext create_sub_ctx(const ParseContext& ctx, const String content, const DynamicArray<Token>& tokens)
{
    ParseContext sub_ctx = ctx;
    sub_ctx.content = content;
    sub_ctx.tokens = tokens;
    sub_ctx.token_index = 0;
    return sub_ctx;
}

static bool parse_tokens(ParseContext& ctx);
static bool store_token_in_var(const Token& token, VariableData& var, ParseContext& ctx);
static bool get_file(ParseContext& ctx, TokenList& out_file);

static VariableData* get_variable_data(ParseContext& ctx)
{
    // It's assumed that the variable is a member of an object
    Token token = ctx.tokens[ctx.token_index];
    const auto& result = find(ctx.parent_var->object, token.string);

    if (!result)
    {
        // Variable doesn't exist yet!
        VariableData var;
        var.type = VariableData::Type::NONE;
        var.name = token.string;
        return &(put(ctx.parent_var->object, token.string, var).value());
    }

    return &(result.value());
}

bool get_variable(ParseContext& ctx, VariableData*& out_var)
{
    bool encountered_error = false;
    const Token& token = ctx.tokens[ctx.token_index];
    
    out_var = get_variable_data(ctx);
    ctx.token_index++;

    {   // Check if members are being refered
        Token next_token = ctx.tokens[ctx.token_index];
        VariableData* prev_parent = ctx.parent_var;
        
        bool keep_parsing = true;
        while (keep_parsing)
        {
            switch (next_token.type)
            {
                case Token::Type::DOT:
                {
                    ctx.token_index++;

                    switch (out_var->type)
                    {
                        case VariableData::Type::OBJECT:
                        {
                            if (ctx.token_index >= ctx.tokens.size)
                            {
                                log_error(ctx.content, next_token.index, "Need a member name after using '.' for an object!");
                                encountered_error = true;
                                break;
                            }

                            next_token = ctx.tokens[ctx.token_index];

                            if (next_token.type != Token::Type::IDENTIFIER)
                            {
                                log_error(ctx.content, token.index, "Expected a member variable name after '.' for object! (found token type: %)", get_token_type_name(next_token.type));
                                encountered_error = true;
                                break;
                            }

                            ctx.parent_var = out_var;
                            out_var = get_variable_data(ctx);
                            ctx.token_index++;
                        } break;

                        case VariableData::Type::ARRAY:
                        {
                            if (ctx.token_index >= ctx.tokens.size)
                            {
                                log_error(ctx.content, token.index, "'.' can only be used after an array for its end!");
                                encountered_error = true;
                                break;
                            }

                            next_token = ctx.tokens[ctx.token_index];

                            if (next_token.type != Token::Type::IDENTIFIER || next_token.string != ref("end"))
                            {
                                log_error(ctx.content, token.index, "'.' can only be used after an array for its end!");
                                encountered_error = true;
                                break;
                            }

                            VariableData end;
                            end.string = ref("end");
                            end.type = VariableData::Type::INTEGER;
                            end.integer = out_var->array.size - 1;
                            
                            append(ctx.var_stack, end);
                            out_var = &ctx.var_stack[ctx.var_stack.size - 1];

                            ctx.token_index++;
                        } break;

                        default:
                        {
                            log_error(ctx.content, next_token.index, "Variable is not an object or array so it can't have any members! (variable type: %)", get_variable_type_name(out_var->type));
                            encountered_error = true;
                            ctx.token_index++;
                        } break;
                    }
                } break;

                case Token::Type::BRACKET_OPEN:
                {
                    ctx.token_index++;

                    if (out_var->type != VariableData::Type::ARRAY)
                    {
                        log_error(ctx.content, next_token.index, "Variable is not an array so it can't be indexed! (variable type: %)", get_variable_type_name(out_var->type));
                        encountered_error = true;
                        break;
                    }

                    if (ctx.token_index >= ctx.tokens.size)
                    {
                        log_error(ctx.content, next_token.index, "Need an index after using [] for an array!");
                        encountered_error = true;
                        break;
                    }

                    next_token = ctx.tokens[ctx.token_index];
                    
                    if (next_token.type != Token::Type::IDENTIFIER && next_token.type != Token::Type::INTEGER)
                    {
                        log_error(ctx.content, token.index, "Expected an integer index inside [] for array! (found token type: %)", get_token_type_name(next_token.type));
                        encountered_error = true;
                        break;
                    }
                    
                    VariableData start;
                    encountered_error = store_token_in_var(next_token, start, ctx);

                    if (start.type != VariableData::Type::INTEGER)
                    {
                        log_error(ctx.content, token.index, "Expected an integer index inside [] for array! (found variable type: %)", get_variable_type_name(start.type));
                        encountered_error = true;
                        break;
                    }

                    if (start.integer < 0 || start.integer >= out_var->array.size)
                    {
                        log_error(ctx.content, token.index, "Index out of bounds! (index: %, array size: %)", start.integer, out_var->array.size);
                        encountered_error = true;
                        break;
                    }

                    next_token = ctx.tokens[ctx.token_index];
                    switch (next_token.type)
                    {
                        case Token::Type::COMMA:
                        {
                            ctx.token_index++;
                            next_token = ctx.tokens[ctx.token_index];
                    
                            if (next_token.type != Token::Type::IDENTIFIER && next_token.type != Token::Type::INTEGER)
                            {
                                log_error(ctx.content, token.index, "Expected an integer index inside [] for array! (found token type: %)", get_token_type_name(next_token.type));
                                encountered_error = true;
                                break;
                            }

                            VariableData end;
                            encountered_error = store_token_in_var(next_token, end, ctx);

                            if (end.type != VariableData::Type::INTEGER)
                            {
                                log_error(ctx.content, token.index, "Expected an integer for upper bound of array! (found variable type: %)", get_variable_type_name(end.type));
                                encountered_error = true;
                                break;
                            }

                            if (end.integer < 0 || end.integer >= out_var->array.size)
                            {
                                log_error(ctx.content, token.index, "Index out of bounds! (index: %, array size: %)", end.integer, out_var->array.size);
                                encountered_error = true;
                                break;
                            }

                            if (start.integer > end.integer)
                            {
                                log_error(ctx.content, token.index, "Range end must be larger than start! (start: %, end: %)", start.integer, end.integer);
                                encountered_error = true;
                                break;
                            }

                            VariableData slice;
                            slice.name = out_var->name;
                            slice.type = VariableData::Type::ARRAY;
                            slice.array.data = out_var->array.data + start.integer;
                            slice.array.size = end.integer - start.integer + 1;
                            slice.array.capacity = out_var->array.capacity - start.integer;

                            append(ctx.var_stack, slice);
                            out_var = &ctx.var_stack[ctx.var_stack.size - 1];

                            next_token = ctx.tokens[ctx.token_index];
                            if (next_token.type != Token::Type::BRACKET_CLOSE)
                            {
                                log_error(ctx.content, next_token.index, "Expected a ']' after array index range! (found token type: %)", get_token_type_name(next_token.type));
                                encountered_error = true;
                            }

                            ctx.token_index++;
                        } break;

                        case Token::Type::BRACKET_CLOSE:
                        {
                            out_var = &out_var->array[start.integer];
                            ctx.token_index++;
                        } break;

                        default:
                        {
                            log_error(ctx.content, next_token.index, "Expected a ',' or ']' after array index! (found token type: %)", get_token_type_name(next_token.type));
                            encountered_error = true;
                            ctx.token_index++;
                        } break;
                    }
                } break;

                default:
                {
                    keep_parsing = false;
                } break;
            }
            
            next_token = ctx.tokens[ctx.token_index];
        }

        ctx.parent_var = prev_parent;
    }

    return encountered_error;
}

static bool store_token_in_var(const Token& token, VariableData& var, ParseContext& ctx)
{
    bool encountered_error = false;

    switch (token.type)
    {
        case Token::Type::STRING:
        {
            var.string = get_token_type_name(token.type);
            var.type = VariableData::Type::STRING;
            var.string = token.string;
            ctx.token_index++;
        } break;
        
        case Token::Type::INTEGER:
        {
            var.string = get_token_type_name(token.type);
            var.type = VariableData::Type::INTEGER;
            var.integer = token.integer;
            ctx.token_index++;
        } break;
        
        case Token::Type::BOOLEAN:
        {
            var.string = get_token_type_name(token.type);
            var.type = VariableData::Type::BOOLEAN;
            var.integer = token.boolean;
            ctx.token_index++;
        } break;

        case Token::Type::TOKENS:
        {
            var.string = get_token_type_name(token.type);
            var.type = VariableData::Type::TOKEN_LIST;
            var.token_list.tokens = token.tokens;
            var.token_list.content = ctx.content;
            ctx.token_index++;
        } break;
        
        case Token::Type::IDENTIFIER:
        {
            VariableData* rhs;
            encountered_error = get_variable(ctx, rhs);
            var = *rhs;
        } break;

        case Token::Type::FILE:
        {
            ctx.token_index++;
            
            var.string = get_token_type_name(token.type);
            var.type = VariableData::Type::TOKEN_LIST;
            encountered_error = get_file(ctx, var.token_list);
        } break;

        default:
        {
            log_error(ctx.content, token.index, "Can't use assignment when rhs has '%'!", get_token_type_name(token.type));
            encountered_error = true;
            ctx.token_index++;
        } break;
    }

    return encountered_error;
}

static bool append_variable(const ParseContext& ctx, const VariableData* var)
{
    bool encountered_error = false;

    switch (var->type)
    {
        case VariableData::Type::BOOLEAN:
        {
            append(ctx.builder, var->boolean ? ref("true") : ref("false"));
        } break;

        case VariableData::Type::INTEGER:
        {
            String number_string = make<String>("12312312313123123");
            to_string(number_string, var->integer);
            append(ctx.builder, number_string);
        } break;

        case VariableData::Type::STRING:
        {
            append(ctx.builder, var->string);
        } break;

        case VariableData::Type::TOKEN_LIST:
        {
            encountered_error = parse_tokens(create_sub_ctx(ctx, var->token_list.content, var->token_list.tokens));
        } break;

        case VariableData::Type::NONE:
        {
            const Token& current_token = ctx.tokens[ctx.token_index];
            log_error(ctx.content, current_token.index, "Variable '%' doesn't exist!", var->name);
            encountered_error = true;
        } break;

        default:
        {
            const Token& current_token = ctx.tokens[ctx.token_index];
            log_error(ctx.content, current_token.index, "Can't convert variable of % type to string for formatting! (variable: %)", get_variable_type_name(var->type), var->name);
            encountered_error = true;
        } break;
    }

    return encountered_error;
}

static inline bool evaluate_variable(const VariableData& var)
{
    if (var.type == VariableData::Type::BOOLEAN)
        return var.boolean;
        
    if (var.type == VariableData::Type::INTEGER)
        return var.integer;
    
    return var.type != VariableData::Type::NONE;
}

static inline bool combine_evaluation(const VariableData var1, const VariableData& var2, VariableData& out_var, Token operation, const ParseContext& ctx)
{
    out_var.type = VariableData::Type::BOOLEAN;
    out_var.boolean = false; // false by default :P

    bool encountered_error = false;
    switch (operation.type)
    {
        case Token::Type::AND:
        {
            out_var.boolean = evaluate_variable(var1) && evaluate_variable(var2);
        } break;
        
        case Token::Type::OR:
        {
            out_var.boolean = evaluate_variable(var1) || evaluate_variable(var2);
        } break;
        
        case Token::Type::EQUAL:
        {
            out_var.boolean = variable_equal(var1, var2);
        } break;

        default:
        {
            log_error(ctx.content, operation.index, "Not a valid operator! (operator token type: %)", get_token_type_name(operation.type));
            encountered_error = true;
        } break;
    }

    return encountered_error;
}

// IMPORTANT! The bool returned here denotes whether an error was encountered or not and not the evaluation.
// The evaluation is stored in out_var.
static inline bool parse_evaluation(ParseContext& ctx, Token start_token)
{
    append(ctx.op_stack, start_token);  // This denotes where the evaluation started

    bool encountered_error = false;
    bool collecting = true;

    u64 collected_var_count = 0;
    u64 collected_op_count = 0;

    // Collect expression variables and operators
    while (collecting)
    {
        if (ctx.token_index >= ctx.tokens.size)
        {
            log_error(ctx.content, ctx.tokens[ctx.token_index - 1].index, "If condition was never closed!");
            encountered_error = true;
            break;
        }

        {   // Get value for condition
            Token next_token = ctx.tokens[ctx.token_index];
            switch (next_token.type)
            {
                case Token::Type::BOOLEAN:
                case Token::Type::INTEGER:
                case Token::Type::STRING:
                {
                    VariableData var;
                    store_token_in_var(next_token, var, ctx);
                    append(ctx.var_stack, var);
                    collected_var_count++;
                } break;

                case Token::Type::IDENTIFIER:
                {
                    VariableData* var;
                    encountered_error = get_variable(ctx, var);
                    append(ctx.var_stack, *var);
                    collected_var_count++;
                } break;

                default:
                {
                    log_error(ctx.content, next_token.index, "Expected a boolean or variable after if! (found token type: %)", get_token_type_name(next_token.type));
                    encountered_error = true;
                    ctx.token_index++;
                } break;
            }
        }

        {   // Get next operation if available
            Token next_token = ctx.tokens[ctx.token_index];
            switch (next_token.type)
            {
                case Token::Type::OR:
                case Token::Type::AND:
                case Token::Type::EQUAL:
                {
                    append(ctx.op_stack, next_token);
                    collected_op_count++;
                    ctx.token_index++;
                } break;

                case Token::Type::TOKENS:
                {
                    collecting = false;
                } break;

                default:
                {
                    log_error(ctx.content, next_token.index, "Expected an operator or fmt block after condition in if tag! (found token type: %)", get_token_type_name(next_token.type));
                    encountered_error = true;
                    ctx.token_index++;
                } break;
            }
        }
    }

    if (collected_var_count != collected_op_count + 1)
    {
        log_error(ctx.content, ctx.tokens[ctx.token_index - 1].index, "Incorrect number of operators and operands in if tag! (operators: %, operands: %)", collected_op_count, collected_var_count);
        return true;
    }

    // Evaluate expression till you reach where you started
    while (ctx.op_stack[ctx.op_stack.size - 1].index != start_token.index)
    {
        Token current_op = pop(ctx.op_stack);

        VariableData var1 = pop(ctx.var_stack);
        VariableData var2 = pop(ctx.var_stack);

        VariableData res = {};
        encountered_error = combine_evaluation(var1, var2, res, current_op, ctx);

        append(ctx.var_stack, res);
    }

    pop(ctx.op_stack);  // Pop the if op token
    return encountered_error;
}

inline bool parse_decision_tree(ParseContext& ctx, DynamicArray<Token>& out_tokens_to_be_executed)
{
    bool encountered_error = false;

    const Token& token = ctx.tokens[ctx.token_index];
    ctx.token_index++;

    switch (token.type)
    {
        case Token::Type::TOKENS:
        {
            out_tokens_to_be_executed = token.tokens;
            return false;
        } break;

        case Token::Type::IF:
        {
            encountered_error = parse_evaluation(ctx, token);
            VariableData evaluation = pop(ctx.var_stack);

            Token next_token = ctx.tokens[ctx.token_index]; // parse_evaluation() already checks for errors for not having a tokens after condition
            bool is_evaluation_true = evaluate_variable(evaluation);
            ctx.token_index++;
            
            if (is_evaluation_true)
            {
                out_tokens_to_be_executed = next_token.tokens;

                // Skip to end of tag
                Token next_token = ctx.tokens[ctx.token_index];
                while (next_token.type != Token::Type::FMT_END)
                {
                    ctx.token_index++;
                    next_token = ctx.tokens[ctx.token_index];
                    
                    if (ctx.token_index >= ctx.tokens.size)
                    {
                        log_error(ctx.content, token.index, "If tag was never closed!");
                        encountered_error = true;
                        break;
                    }
                }

                return encountered_error;
            }

            next_token = ctx.tokens[ctx.token_index];
            switch (next_token.type)
            {
                case Token::Type::ELSE:
                {
                    if (!is_evaluation_true)
                    {
                        ctx.token_index++;
                        parse_decision_tree(ctx, out_tokens_to_be_executed);
                    }
                } break;

                case Token::Type::FMT_END:
                {
                    out_tokens_to_be_executed = {};
                } break;

                default:
                {
                    log_error(ctx.content, token.index, "Unexpected token in if tag %!", get_token_type_name(token.type));
                    encountered_error = true;
                    ctx.token_index++;
                } break;
            }
        } break;

        default:
        {
            log_error(ctx.content, token.index, "Unexpected token in if tag %!", get_token_type_name(token.type));
            encountered_error = true;
            ctx.token_index++;
        } break;
    }

    return encountered_error;
}

static bool get_file(ParseContext& ctx, TokenList& out_file)
{
    Token next_token = ctx.tokens[ctx.token_index];
    VariableData file_path_var;
    bool encountered_error = store_token_in_var(next_token, file_path_var, ctx);

    if (file_path_var.type != VariableData::Type::STRING)
    {
        log_error(ctx.content, next_token.index, "File expects a string for the file path! (found variable type: %)", get_variable_type_name(file_path_var.type));
        encountered_error = true;
    }

    // Check if file has already been tokenized
    const auto& res = find(ctx.tokenized_files, file_path_var.string);
    if (res)
    {
        out_file = res.value();
        return out_file.encountered_error;
    }

    // Load file
    TokenList file = {};
    file.content = file_load_string(file_path_var.string);
    file.encountered_error = !tokenize(file.content, file.tokens);
    out_file = put(ctx.tokenized_files, file_path_var.string, file).value();

    return out_file.encountered_error;
}

// Returns true if any errors are encountered
bool parse_format_tag(ParseContext& ctx)
{
    ctx.token_index++;  // Skip fmt start

    bool encountered_error = false;
    bool in_fmt_tag = true;

    u64 var_stack_start_size = ctx.var_stack.size;
    u64 op_stack_start_size  = ctx.op_stack.size;

    while (in_fmt_tag)
    {
        const Token& token = ctx.tokens[ctx.token_index];
        switch (token.type)
        {
            case Token::Type::FMT_END:
            {
                // Since it's all references or POD types so no need to free anything
                ctx.var_stack.size = var_stack_start_size;
                ctx.op_stack.size = op_stack_start_size;

                in_fmt_tag = false;
                ctx.token_index++;
            } break;

            case Token::Type::IDENTIFIER:
            {
                VariableData* var;
                encountered_error = get_variable(ctx, var);

                Token next_token = ctx.tokens[ctx.token_index];
                switch (next_token.type)
                {
                    case Token::Type::COLON:
                    {
                        ctx.token_index++;

                        next_token = ctx.tokens[ctx.token_index];
                        store_token_in_var(next_token, *var, ctx);
                    } break;

                    case Token::Type::FMT_END:
                    {
                        // Use the variable here!
                        append_variable(ctx, var);
                    } break;

                    default:
                    {
                        log_error(ctx.content, next_token.index, "Variables can only be followed by '$>' or ':'");
                        encountered_error = true;
                        ctx.token_index++;
                    } break;
                }

            } break;

            case Token::Type::IF:
            {
                DynamicArray<Token> execute_tokens;
                encountered_error = parse_decision_tree(ctx, execute_tokens);
                encountered_error = parse_tokens(create_sub_ctx(ctx, ctx.content, execute_tokens));
            } break;
            
            case Token::Type::FOR:
            {
                ctx.token_index++;

                VariableData* var;
                encountered_error = get_variable(ctx, var);

                Token next_token = ctx.tokens[ctx.token_index];

                VariableData* index = {};
                if (next_token.type == Token::Type::COMMA)
                {
                    // Need an index too
                    ctx.token_index++;
                    encountered_error = get_variable(ctx, index);
                    index->type = VariableData::Type::INTEGER;

                    next_token = ctx.tokens[ctx.token_index];
                }

                switch (next_token.type)
                {
                    case Token::Type::COLON:
                    {
                        ctx.token_index++;
                        if (ctx.token_index >= ctx.tokens.size)
                        {
                            log_error(ctx.content, token.index, "For tag was never closed!");
                            encountered_error = true;
                            break;
                        }
                        
                        VariableData* array_var;
                        encountered_error = get_variable(ctx, array_var);

                        if (array_var->type != VariableData::Type::ARRAY)
                        {
                            log_error(ctx.content, next_token.index, "For loops can only loop over arrays! (found variable type: %)", get_variable_type_name(array_var->type));
                            encountered_error = true;
                            break;
                        }

                        if (ctx.token_index >= ctx.tokens.size)
                        {
                            log_error(ctx.content, token.index, "For tag was never closed!");
                            encountered_error = true;
                            break;
                        }

                        next_token = ctx.tokens[ctx.token_index];
                        if (next_token.type != Token::Type::TOKENS)
                        {
                            log_error(ctx.content, next_token.index, "For tag needs a fmt body to be executed every iteration of the loop");
                            encountered_error = true;
                            break;
                        }

                        DynamicArray<Token> loop_body = next_token.tokens;

                        for (u64 i = 0; i < array_var->array.size; i++)
                        {
                            *var = array_var->array[i];

                            if (index)
                                index->integer = i;

                            encountered_error = parse_tokens(create_sub_ctx(ctx, ctx.content, loop_body));
                        }

                        ctx.token_index++;
                    } break;

                    default:
                    {
                        log_error(ctx.content, next_token.index, "Variables can only be followed by '$>' or ':'");
                        encountered_error = true;
                        ctx.token_index++;
                    } break;
                }
            } break;
            
            case Token::Type::FILE:
            {
                ctx.token_index++;

                TokenList file;
                encountered_error = get_file(ctx, file);
                if (encountered_error)
                    break;

                encountered_error = parse_tokens(create_sub_ctx(ctx, file.content, file.tokens));
            } break;

            default:
            {
                log_error(ctx.content, token.index, "Fmt tag can't start with %!", get_token_type_name(token.type));
                encountered_error = true;
                ctx.token_index++;
            } break;
        }
    }

    return encountered_error;
}

// Returns true if any errors are encountered
static bool parse_tokens(ParseContext& ctx)
{
    bool encountered_error = false;

    while (ctx.token_index < ctx.tokens.size)
    {
        const Token& token = ctx.tokens[ctx.token_index];
        switch (token.type)
        {
            case Token::Type::RAW_STRING:
            {
                append(ctx.builder, token.string);
                ctx.token_index++;
            } break;

            case Token::Type::FMT_START:
            {
                encountered_error = parse_format_tag(ctx);
            } break;

            default:
            {
                log_error(ctx.content, token.index, "Expected either a raw string of a fmt tag! (current token: %)", get_token_type_name(token.type));
                encountered_error = true;
                ctx.token_index++;
            } break;
        }
    }

    return encountered_error;
}

// Assumes that pass.tokens is filled already
bool parse_template(const String content, Pass& pass, StringBuilder& builder)
{
    clear(builder);
    if (builder.capacity <= 2)
        resize(builder, max(2ui64, content.size / 10)); // Just an estimate

    ParseContext ctx = { content, builder, pass.tokens, &pass.root_var };

    // Only need to create it once!
    if (ctx.tokenized_files.capacity == 0)
        ctx.tokenized_files = make<HashTable<String, TokenList>>();

    clear(ctx.var_stack);
    if (ctx.var_stack.capacity <= 2)
        resize(ctx.var_stack, 32);

    clear(ctx.op_stack);
    if (ctx.op_stack.capacity <= 2)
        resize(ctx.op_stack, 32);

    return !parse_tokens(ctx);
}

} // namespace Fmt