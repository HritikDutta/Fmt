#include "json_parser.h"

#define SLZ_ERROR_PREFIX "Json"

#include "core/types.h"
#include "core/logger.h"
#include "serialization/slz/slz_debug_output.h"
#include "serialization/slz/slz_error.h"
#include "serialization/slz.h"
#include "json_lexer.h"

namespace Json
{

struct ParserContext
{
    String content;
    u64 current_index;
    bool encountered_error;
};

static inline bool is_token_a_value(const ParserContext& context, const Token& token)
{
    switch (token.type)
    {
        case Token::Type::IDENTIFIER:
        case Token::Type::INTEGER:
        case Token::Type::FLOAT:
        case Token::Type::STRING:
        case Token::Type::BRACKET_OPEN:
        case Token::Type::BRACE_OPEN:
            return true;
        
        case Token::Type::COLON:
        case Token::Type::COMMA:
        case Token::Type::BRACE_CLOSE:
        case Token::Type::BRACKET_CLOSE:
            return false;

        default:
            log_error(context.content, token.index,"Incorrect token type! (found: '%')", (s32) token.type);
    }

    return false;
}

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

static void parse_next(const DynamicArray<Token>& tokens, ParserContext& context, Slz::Document& out)
{
    if (context.current_index > tokens.size)
    {
        log_error(context.content, tokens.size - 1, "Json data is incomplete! (Parser ran out of tokens)");
        context.encountered_error = true;
        return;
    }

    const Token& token = tokens[context.current_index];
    switch (token.type)
    {
        case Token::Type::STRING:
        {
            Slz::DependencyNode node = {};
            node.index = out.resources.size;
            node.type  = Slz::Type::STRING;
            append(out.dependency_tree, node);

            Slz::Resource res = {};
            res.string = copy_and_escape(token, context);
            append(out.resources, res);
        } break;

        case Token::Type::INTEGER:
        {
            Slz::DependencyNode node = {};
            node.index = out.resources.size;
            node.type  = Slz::Type::INTEGER;
            append(out.dependency_tree, node);

            // TODO: convert string to integer on your own with error checking
            Slz::Resource res = {};
            res.integer64 = _atoi64(token.value.data);
            append(out.resources, res);
        } break;

        case Token::Type::FLOAT:
        {
            Slz::DependencyNode node = {};
            node.index = out.resources.size;
            node.type  = Slz::Type::FLOAT;
            append(out.dependency_tree, node);

            // TODO: convert string to float on your own with error checking
            Slz::Resource res = {};
            res.float64 = atof(token.value.data);
            append(out.resources, res);
        } break;

        case Token::Type::IDENTIFIER:
        {
            Slz::ResourceIndex index = out.resources.size;
            Slz::Type node_type = Slz::Type::BOOLEAN;

            {   // Determine type of resource
                if (token.value == ref("null", 4))
                {
                    // Point to null value in dependency tree
                    node_type = Slz::Type::NONE;
                    index = 0;
                }
                else if (token.value == ref("true", 4))
                {
                    // Point to true value in dependency tree
                    index = 2;
                }
                else if (token.value == ref("false", 5))
                {
                    // Point to false value in dependency tree
                    index = 1;
                }
                else
                {
                    // Can't identify the identifier, lol
                    log_error(context.content, token.index, "Identifiers can only be true, false, or null! (found: '%')", token.value);
                    context.encountered_error = true;
                }
            }

            Slz::DependencyNode node = {};
            node.index = index;
            node.type  = node_type;
            append(out.dependency_tree, node);
        } break;

        case Token::Type::BRACKET_OPEN:
        {
            u64 array_tree_index = out.dependency_tree.size;

            Slz::DependencyNode node = {};
            node.array = make<Slz::ArrayNode>();
            node.type  = Slz::Type::ARRAY;
            append(out.dependency_tree, node);

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

                // Closed array
                if (tokens[context.current_index].type == Token::Type::BRACKET_CLOSE)
                    break;

                append(out.dependency_tree[array_tree_index].array, out.dependency_tree.size);
                parse_next(tokens, context, out);

                if (context.current_index >= tokens.size)
                {
                    log_error(context.content, tokens[context.current_index - 1].index, "Array was never closed with a ]!");
                    context.encountered_error = true;
                    break;
                }

                const Token& current_token = tokens[context.current_index];

                // Closed array
                if (current_token.type == Token::Type::BRACKET_CLOSE)
                    break;

                if (current_token.type != Token::Type::COMMA)
                {
                    log_error(context.content, current_token.index, "Array items must be separated by commas! (found: '%')", current_token.value);
                    context.encountered_error = true;

                    // Don't skip over values. Helps with error checking.
                    if (is_token_a_value(context, current_token))
                        context.current_index--;
                }

                context.current_index++;
            }

            // ] gets skipped at the end anyways
        } break;

        case Token::Type::BRACE_OPEN:
        {
            u64 object_tree_index = out.dependency_tree.size;

            Slz::DependencyNode node = {};
            node.object = make<Slz::ObjectNode>();
            node.type  = Slz::Type::OBJECT;
            append(out.dependency_tree, node);

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

                const Token& key_token = tokens[context.current_index];

                // Closed object
                if (key_token.type == Token::Type::BRACE_CLOSE)
                    break;

                if (key_token.type != Token::Type::STRING)
                {
                    log_error(context.content, key_token.index, "Expected a key for object! (found: '%')", key_token.value);
                    context.encountered_error = true;
                }

                context.current_index++;
                const Token& colon_token = tokens[context.current_index];

                // Key should be followed by a :
                if (colon_token.type != Token::Type::COLON)
                {
                    log_error(context.content, colon_token.index, "Expected : after key in object! (found: '%')", colon_token.value);
                    context.encountered_error = true;

                    // Don't skip over values. Helps with error checking.
                    if (is_token_a_value(context, colon_token))
                        context.current_index--;
                }

                String key_string = copy_and_escape(key_token, context);
                put(out.dependency_tree[object_tree_index].object, key_string, out.dependency_tree.size);

                context.current_index++;
                parse_next(tokens, context, out);

                if (context.current_index >= tokens.size)
                {
                    log_error(context.content, tokens[context.current_index - 1].index, "Object was never closed with a }!");
                    context.encountered_error = true;
                    break;
                }
                
                // Closed object
                const Token& next_token = tokens[context.current_index];
                if (next_token.type == Token::Type::BRACE_CLOSE)
                    break;
                
                if (next_token.type != Token::Type::COMMA)
                {
                    log_error(context.content, next_token.index, "Object properties must be separated by commas! (found: '%')", next_token.value);
                    context.encountered_error = true;

                    // Don't skip over values. Helps with error checking.
                    if (is_token_a_value(context, next_token))
                        context.current_index--;
                }

                context.current_index++;
            }

            // } gets skipped at the end anyways
        } break;

        default:
        {
            // The only remaining tokens are single character punctuations
            log_error(context.content, token.index, "Expected a value (identifier, number, string, array, or object), got %", (char) token.type);
            context.encountered_error = true;
        } break;
    }

    context.current_index++;
}

bool parse_tokens(const DynamicArray<Token>& tokens, const String content, Slz::Document& out)
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
    
    parse_next(tokens, context, out);

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

bool parse_string(const String content, Slz::Document& out)
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

} // namespace Json
