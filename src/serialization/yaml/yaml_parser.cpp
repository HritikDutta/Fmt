#include "yaml_parser.h"

#define SLZ_ERROR_PREFIX "Yaml"

#include "core/logger.h"
#include "core/types.h"
#include "core/utils.h"
#include "containers/string.h"
#include "containers/string_builder.h"
#include "serialization/slz/slz_debug_output.h"
#include "serialization/slz/slz_error.h"
#include "serialization/slz.h"
#include "yaml_lexer.h"

namespace Yaml
{

struct ParserContext
{
    String content;
    u64 current_index;
    bool encountered_error;
};

struct IndentContext
{
    s64 line, indentation;
};

static String copy_and_escape(const Token& source_token, ParserContext& context)
{
    DynamicArray<char> result = make<DynamicArray<char>>(source_token.value.size);

    for (u64 i = 0; i < source_token.value.size; i++)
    {
        char ch = source_token.value[i];

        if (ch == '\\')
        {
            i++;
            switch (source_token.value[i])
            {
                case 'b':  ch = '\b'; break;
                case 'f':  ch = '\f'; break;
                case 'n':  ch = '\n'; break;
                case 'r':  ch = '\r'; break;
                case 't':  ch = '\t'; break;
                case '\"': ch = '\"'; break;
                case '\\': ch = '\\'; break;

                default:
                {
                    log_error(context.content, source_token.index, "Unexpected escape character! (character: '\\%')", source_token.value[i]);
                    context.encountered_error = true;
                } break;
            }
        }

        append(result, ch);
    }

    return String { result.data, result.size };
}

static Slz::ResourceIndex parse_next(const DynamicArray<Token>& tokens, ParserContext& context, const IndentContext& indent_ctx, Slz::Document& out)
{
    Slz::ResourceIndex current_node_index = out.dependency_tree.size;
    append(out.dependency_tree, {});

    const Token& token = tokens[context.current_index];
    switch (token.type)
    {
        case Token::Type::SCALAR:
        {
            Slz::DependencyNode& node = out.dependency_tree[current_node_index];

            if (token.value == ref("null"))
            {
                // Point to null value in dependency tree
                node.index = 0;
                node.type = Slz::Type::NONE;
                context.current_index++;
                break;
            }

            if (token.value == ref("true"))
            {
                // Point to true value in dependency tree
                node.index = 2;
                node.type = Slz::Type::BOOLEAN;
                context.current_index++;
                break;
            }

            if (token.value == ref("false"))
            {
                // Point to false value in dependency tree
                node.index = 1;
                node.type = Slz::Type::BOOLEAN;
                context.current_index++;
                break;
            }

            // Check if it's a number
            if (!token.is_quoted && (token.value[0] == '-' || token.value[0] == '.' || is_digit(token.value[0])))
            {
                bool encountered_dot = token.value[0] == '.';
                bool is_number = true;

                for (u64 i = 1; i < token.value.size; i++)
                {
                    // - is not allowed between numbers (no math allowed!)
                    if (token.value[i] == '-')
                    {
                        is_number = false;
                        break;
                    }
                    
                    // there should be only 1 dot in a number
                    if (token.value[i] == '.')
                    {
                        if (encountered_dot)
                        {
                            is_number = false;
                            break;
                        }

                        encountered_dot = true;
                    }
                    else if (!is_digit(token.value[i]))
                    {
                        is_number = false;
                        break;
                    }
                }

                if (is_number)
                {
                    node.index = out.resources.size;

                    Slz::Resource res = {};

                    if (encountered_dot)
                    {
                        res.float64 = atof(token.value.data);
                        node.type = Slz::Type::FLOAT;
                    }
                    else
                    {
                        res.integer64 = _atoi64(token.value.data);
                        node.type = Slz::Type::INTEGER;
                    }

                    append(out.resources, res);
                    context.current_index++;
                    break;
                }
            }

            // Append scalar as a string
            Slz::ResourceIndex index = out.resources.size;

            Slz::Resource res = {};
            res.string = token.is_quoted ? copy_and_escape(token, context) : copy(token.value);
            append(out.resources, res);

            node.index = index;
            node.type  = Slz::Type::STRING;

            context.current_index++;
        } break;

        case Token::Type::KEY:
        {
            if (token.line < indent_ctx.line)
            {
                log_error(context.content, token.index, "Object member can't start at the same line as the parent!");
                context.encountered_error = true;
                break;
            }

            // If the token's indentation is equal to or lower than the parent that means it can't be a child
            if (token.indentation <= indent_ctx.indentation)
                break;

            if (out.dependency_tree[current_node_index].type != Slz::Type::NONE)
            {
                log_error(context.content, token.index, "Trying to insert a wrong element into an object! It must be a key value pair! (found token: %)", token.value);
                context.encountered_error = true;
                break;
            }

            {   // Initialize parent as an object
                Slz::DependencyNode& node = out.dependency_tree[current_node_index];
                node.object = make<Slz::ObjectNode>();
                node.type = Slz::Type::OBJECT;
            }

            while (context.current_index < tokens.size)
            {
                const Token& next_token = tokens[context.current_index];

                if (next_token.indentation < token.indentation)
                    break;
                
                if (next_token.indentation != token.indentation)
                {
                    log_error(context.content, next_token.index, "Incorrect indentation!");
                    context.encountered_error = true;
                    break;
                }

                if (next_token.type != Token::Type::KEY)
                {
                    log_error(context.content, next_token.index, "Expected a key for object! (found: '%', type: %)", next_token.value, get_token_type_name(next_token.type));
                    context.encountered_error = true;
                    break;
                }

                const String& key_string = next_token.is_quoted ? copy_and_escape(next_token, context) : next_token.value;

                context.current_index++;

                // Check if the key is followed up by another key or if the document ends here
                // If so then assign null to this key
                if (context.current_index >= tokens.size || (tokens[context.current_index].type == Token::Type::KEY && tokens[context.current_index].indentation == token.indentation))
                {
                    Slz::ResourceIndex value_index = out.dependency_tree.size;
                    append(out.dependency_tree, {});
                    put(out.dependency_tree[current_node_index].object, key_string, value_index);

                    continue;
                }

                IndentContext child_indent_ctx = { next_token.line, next_token.indentation };
                Slz::ResourceIndex value_index = parse_next(tokens, context, child_indent_ctx, out);
                put(out.dependency_tree[current_node_index].object, key_string, value_index);
            }
        } break;

        // Objects as a flow collection (Json Style)
        case Token::Type::BRACE_OPEN:
        {
            {   // Initialize parent as an object
                Slz::DependencyNode& node = out.dependency_tree[current_node_index];
                node.object = make<Slz::ObjectNode>();
                node.type = Slz::Type::OBJECT;
            }
            
            // Skip the first {
            context.current_index++;

            while (true)
            {
                if (context.current_index >= tokens.size)
                {
                    log_error(context.content, tokens[context.current_index - 1].index, "Object was never closed with a }!");
                    context.encountered_error = true;
                    break;
                }

                Token next_token = tokens[context.current_index];
                
                // Closed object
                if (next_token.type == Token::Type::BRACE_CLOSE)
                {
                    context.current_index++;
                    break;
                }
                
                if (next_token.type != Token::Type::KEY)
                {
                    log_error(context.content, next_token.index, "Expected a key for object! (found: '%', type: %)", next_token.value, get_token_type_name(next_token.type));
                    context.encountered_error = true;
                    break;
                }

                const String& key_string = next_token.is_quoted ? copy_and_escape(next_token, context) : next_token.value;

                context.current_index++;

                IndentContext child_indent_ctx = { next_token.line, next_token.indentation };
                Slz::ResourceIndex value_index = parse_next(tokens, context, child_indent_ctx, out);
                put(out.dependency_tree[current_node_index].object, key_string, value_index);

                if (context.current_index >= tokens.size)
                {
                    log_error(context.content, tokens[context.current_index - 1].index, "Object was never closed with a }!");
                    context.encountered_error = true;
                    break;
                }
                
                // Closed object
                next_token = tokens[context.current_index];
                if (next_token.type == Token::Type::BRACE_CLOSE)
                {
                    context.current_index++;
                    break;
                }
                    
                if (next_token.type != Token::Type::COMMA)
                {
                    log_error(context.content, next_token.index, "Object properties must be separated by commas! (found: '%')", next_token.value);
                    context.encountered_error = true;
                    break;
                }

                context.current_index++;
            }
        } break;

        case Token::Type::DASH:
        {
            if (token.line <= indent_ctx.line)
            {
                log_error(context.content, token.index, "Array item can't start at the same line as the parent!");
                context.encountered_error = true;
                break;
            }

            // If the token's indentation is lower than the parent that means it can't be an element in this list
            if (token.indentation < indent_ctx.indentation)
                break;

            {   // Initialize parent as an array
                Slz::DependencyNode& node = out.dependency_tree[current_node_index];
                node.array = make<Slz::ArrayNode>();
                node.type = Slz::Type::ARRAY;
            }

            while (context.current_index < tokens.size)
            {
                const Token& next_token = tokens[context.current_index];

                if (next_token.indentation < token.indentation)
                    break;
                
                if (next_token.indentation != token.indentation)
                {
                    log_error(context.content, next_token.index, "Incorrect indentation!");
                    context.encountered_error = true;
                    break;
                }

                if (next_token.type != Token::Type::DASH)
                {
                    log_error(context.content, next_token.index, "Expected a list item for array! (found: '%', type: %)", next_token.value, get_token_type_name(next_token.type));
                    context.encountered_error = true;
                    break;
                }

                context.current_index++;

                // Check if the list dash is followed up by another one or if the document ends here
                // If so then append null to this list
                if (context.current_index >= tokens.size || (tokens[context.current_index].type == Token::Type::DASH && tokens[context.current_index].indentation == token.indentation))
                {
                    Slz::ResourceIndex value_index = out.dependency_tree.size;
                    append(out.dependency_tree, {});
                    append(out.dependency_tree[current_node_index].array, value_index);
                    
                    continue;
                }
                
                IndentContext child_indent_ctx = { next_token.line, next_token.indentation };
                Slz::ResourceIndex value_index = parse_next(tokens, context, child_indent_ctx, out);
                append(out.dependency_tree[current_node_index].array, value_index);
            }
        } break;

        // Arrays as a flow collection (Json Style)
        case Token::Type::BRACKET_OPEN:
        {
            {   // Initialize parent as an array
                Slz::DependencyNode& node = out.dependency_tree[current_node_index];
                node.array = make<Slz::ArrayNode>();
                node.type = Slz::Type::ARRAY;
            }
            
            // Skip the first [
            context.current_index++;

            while (true)
            {
                if (context.current_index >= tokens.size)
                {
                    log_error(context.content, tokens[context.current_index - 1].index, "Array was never closed with a ]!");
                    context.encountered_error = true;
                    break;
                }
                
                Token next_token = tokens[context.current_index];
                
                // Closed array
                if (next_token.type == Token::Type::BRACKET_CLOSE)
                {
                    context.current_index++;
                    break;
                }
                    
                IndentContext child_indent_ctx = { next_token.line, next_token.indentation };
                Slz::ResourceIndex value_index = parse_next(tokens, context, child_indent_ctx, out);
                append(out.dependency_tree[current_node_index].array, value_index);

                if (context.current_index >= tokens.size)
                {
                    log_error(context.content, tokens[context.current_index - 1].index, "Array was never closed with a ]!");
                    context.encountered_error = true;
                    break;
                }

                next_token = tokens[context.current_index];

                // Closed array
                if (next_token.type == Token::Type::BRACKET_CLOSE)
                {
                    context.current_index++;
                    break;
                }
                    
                if (next_token.type != Token::Type::COMMA)
                {
                    log_error(context.content, next_token.index, "Array items must be separated by commas! (found: '%', type: %)", next_token.value, get_token_type_name(next_token.type));
                    context.encountered_error = true;
                    break;
                }

                context.current_index++;
            }
        } break;
    }

    return current_node_index;
}

bool parse_tokens(const DynamicArray<Token> &tokens, const String content, Slz::Document &out)
{
    gn_assert_with_message(tokens.data, "Tokens array points to null!");
    gn_assert_with_message(out.dependency_tree.size == 0, "Output json Slz::Document struct is not empty! (number of elements: %)", out.dependency_tree.size);

    {   // Add the null element
        // If user tries to access an object property that wasn't in the file,
        // then the value will point to this element
        append(out.dependency_tree, Slz::DependencyNode {});
        append(out.resources, Slz::Resource {});
    }

    if (tokens.size == 0)
    {
        log_error(content, 0, "Tokens array is empty!");
        return false;
    }

    {   // Add constants for true and false
        Slz::DependencyNode node;
        node.type  = Slz::Type::BOOLEAN;

        Slz::Resource res;

        {
            node.index = out.resources.size;
            res.boolean = false;

            append(out.dependency_tree, node);
            append(out.resources, res);
        }

        {
            node.index = out.resources.size;
            res.boolean = true;

            append(out.dependency_tree, node);
            append(out.resources, res);
        }
    }

    ParserContext context = {};
    context.content = content;

    IndentContext indent_ctx;
    indent_ctx.line = indent_ctx.indentation = -1;
    
    parse_next(tokens, context, indent_ctx, out);

    if (!context.encountered_error && context.current_index < tokens.size)
    {
        log_error(context.content, tokens[context.current_index].index, "End of file expected! (found: '%')", tokens[context.current_index].value);
        context.encountered_error = true;
    }
    
    #ifdef GN_LOG_SERIALIZATION
        if (!context.encountered_error)
            Slz::document_debug_output(out);
    #endif // GN_LOG_SERIALIZATION

    return !context.encountered_error;
}

bool parse_string(const String content, Slz::Document &out)
{
    DynamicArray<Token> tokens = {};
    bool success = tokenize(content, tokens);

    if (!success)
    {
        print_error("Lexing failed!");
        goto err_lexing;
    }

    success = parse_tokens(tokens, content, out);
    if (!success)
        print_error("Parsing failed!");

err_lexing:
    free(tokens);

    return success;
}

} // namespace Yaml