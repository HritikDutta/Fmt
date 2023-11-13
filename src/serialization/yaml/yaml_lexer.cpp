#include "yaml_lexer.h"

#define SLZ_ERROR_PREFIX "Yaml"

#include "core/types.h"
#include "core/utils.h"
#include "containers/darray.h"
#include "containers/string.h"
#include "containers/string_builder.h"
#include "math/common.h"
#include "serialization/slz/slz_error.h"

namespace Yaml
{

static inline bool is_reserved_char(char ch)
{
    constexpr char reserved_chars[] = "[]{}>|*&!%#`@,";
    constexpr char reserved_chars_count = sizeof(reserved_chars) / sizeof(char);

    for (int i = 0; i < reserved_chars_count; i++)
    {
        if (ch == reserved_chars[i])
            return true;
    }

    return false;
}

static bool eat_spaces_and_get_indentation(const String& content, u64& current_index, s64& out_indentation, s64& out_line)
{
    bool encountered_error = false;
    bool keep_counting = true;

    while (keep_counting && current_index < content.size)
    {
        switch (content[current_index])
        {
            case '\t':
            {
                log_error(content, current_index, "Tabs are not allowed in yaml!");
                encountered_error = true;
                current_index++;
            } break;

            case '\r':
            case '\n':
            {
                // Reset indentation on new line!
                out_indentation = 0;
                out_line += content[current_index] == '\n';
                current_index++;
            } break;

            case ' ':
            {
                out_indentation++;
                current_index++;
            } break;

            case '#':
            {
                // Ignore the line
                while (current_index < content.size && content[current_index] != '\n')
                    current_index++;
            } break;

            default:
            {
                keep_counting = false;
            } break;
        }
    }

    return encountered_error;
}

bool collect_block_string(String& block_string, const String content, char delim, u64& current_index, s64& out_indentation, s64& out_line)
{
    // Next line started
    s64 start_indent = out_indentation;
    s64 start_line = out_line;

    if (eat_spaces_and_get_indentation(content, current_index, start_indent, start_line))
        return true;

    if (start_line <= out_line)
    {
        log_error(content, current_index, "Block literal must start from a new line!");
        return true;
    }
    
    if (start_indent <= out_indentation)
    {
        log_error(content, current_index, "Block literal must be indented more than the parent!");
        return true;
    }
    
    StringBuilder builder = make<StringBuilder>();

    char new_lines[] = "\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n";
    char spaces[] = "                                                                ";

    s64 indentation = start_indent;  // Used for spacing
    s64 line = start_line;   // Used for new lines

    append(builder, ref(new_lines, (u64) (line - out_line - 1)));

    while (current_index < content.size)
    {
        s64 prev_line = line;
        if (eat_spaces_and_get_indentation(content, current_index, indentation, line))
            return true;

        if (indentation < start_indent)
            break;

        append(builder, ref(new_lines, (u64) (max(line - prev_line - 1, 0i64))));
        append(builder, ref(spaces, (u64) (indentation - start_indent)));

        {   // Append string till end of line or file
            u64 str_size = 0;
            while (current_index + str_size < content.size)
            {
                if (content[current_index + str_size] == '\r' || content[current_index + str_size] == '\n')
                    break;
                
                str_size++;
            }

            append(builder, get_substring(content, current_index, str_size));
            append(builder, ref(&delim, 1));
            current_index += str_size;
        }
    }

    block_string = build_string(builder);
    free(builder);

    out_indentation = indentation;
    out_line = line;
    return false;
}

bool tokenize(const String content, DynamicArray<Token> &tokens)
{
    clear(tokens);

    if (tokens.size == 0)
        resize(tokens, max(2ui64, content.size / 10)); // Just an estimate
    
    bool encountered_error = false;
    u64 current_index = 0;

    s64 indentation = 0;
    s64 line = 0;

    bool force_key = false;

    while (true)
    {
        // Won't check for null character to end lexing since strings have a specified length

        // Reached EOF
        if (current_index >= content.size)
            break;

        encountered_error = eat_spaces_and_get_indentation(content, current_index, indentation, line);

        while (current_index < content.size)
        {
            // New line == new context
            if (content[current_index] == '\n')
                break;

            // Other than new line but it's handled above any ways
            if (is_white_space(content[current_index]))
            {
                current_index++;
                continue;
            }

            {   // Single character tokens! (Apart from '-' and ':' since they have more conditions)
                bool found_token = false;
                switch (content[current_index])
                {
                    // Punctuations
                    case (char) Token::Type::BRACKET_OPEN:
                    case (char) Token::Type::BRACKET_CLOSE:
                    case (char) Token::Type::BRACE_OPEN:
                    case (char) Token::Type::BRACE_CLOSE:
                    case (char) Token::Type::COMMA:
                    {
                        Token token;
                        token.index = current_index;
                        token.indentation = indentation;
                        token.line = line;
                        token.type  = (Token::Type) content[current_index];
                        token.value = get_substring(content, current_index, 1);

                        append(tokens, token);
                        current_index++;

                        found_token = true;
                    } break;
                }

                if (found_token)
                    continue;
            }

            if (content[current_index] == '|')
            {
                current_index++;

                Token token;
                token.index = current_index;
                token.indentation = indentation;
                token.line = line;
                token.type  = Token::Type::SCALAR;
                token.is_quoted = true;

                encountered_error = collect_block_string(token.value, content, '\n', current_index, indentation, line);

                append(tokens, token);
                continue;
            }

            if (content[current_index] == '>')
            {
                current_index++;

                Token token;
                token.index = current_index;
                token.indentation = indentation;
                token.line = line;
                token.type  = Token::Type::SCALAR;
                token.is_quoted = true;

                encountered_error = collect_block_string(token.value, content, ' ', current_index, indentation, line);

                append(tokens, token);
                continue;
            }

            if (content[current_index] == '?')
            {
                force_key = true;
                current_index++;
                continue;
            }

            // Quoted String
            if (content[current_index] == '\"')
            {
                // Skip the first "
                current_index++;

                // Eat till the next "
                u64 str_size = 0;
                while (true)
                {
                    const u64 index = current_index + str_size;

                    // Reached EOF before closing string
                    if (index >= content.size)
                    {
                        log_error(content, current_index + str_size - 1, "String was not closed!");
                        encountered_error = true;
                        break;
                    }

                    if (content[index] == '\"')
                        break;

                    // Reached new line before closing string
                    if (content[index] == '\n')
                    {
                        log_error(content, current_index + str_size - 1, "Reached new line before closing string!");
                        encountered_error = true;
                        break;
                    }

                    // Skip the next character if current character is a '\'
                    if (content[index] == '\\')
                        str_size++;

                    str_size++;
                }

                Token token;
                token.index = current_index;
                token.indentation = indentation;
                token.line = line;
                token.type  = force_key ? Token::Type::KEY : Token::Type::SCALAR;
                token.is_quoted = true;
                token.value = get_substring(content, current_index, str_size);

                append(tokens, token);

                // Skip the ending "
                current_index += str_size + 1;
                continue;
            }

            // Scalar String
            if (content[current_index] == '\'')
            {
                // Skip the first "
                current_index++;

                // Eat till the next "
                u64 str_size = 0;
                while (true)
                {
                    const u64 index = current_index + str_size;

                    // Reached EOF before closing string
                    if (index >= content.size)
                    {
                        log_error(content, current_index + str_size - 1, "String was not closed!");
                        encountered_error = true;
                        break;
                    }

                    if (content[index] == '\'')
                        break;

                    // Reached new line before closing string
                    if (content[index] == '\n')
                    {
                        log_error(content, current_index + str_size - 1, "Reached new line before closing string!");
                        encountered_error = true;
                        break;
                    }

                    str_size++;
                }

                Token token;
                token.index = current_index;
                token.indentation = indentation;
                token.line = line;
                token.type  = force_key ? Token::Type::KEY : Token::Type::SCALAR;
                token.value = get_substring(content, current_index, str_size);

                append(tokens, token);

                // Skip the ending "
                current_index += str_size + 1;
                continue;
            }

            // Found a comment '#' preceded by a whitespace (or the first character of the string)
            if (content[current_index] == '#' && (current_index == 0 || is_white_space(content[current_index - 1])))
            {
                // Ignore the line
                while (current_index < content.size && content[current_index] != '\n')
                    current_index++;
                
                continue;
            }

            // List item dash
            if (content[current_index] == '-' && (current_index + 1 >= content.size || is_white_space(content[current_index + 1])))
            {
                Token token;
                token.index = current_index;
                token.indentation = indentation;
                token.line = line;
                token.type  = Token::Type::DASH;
                token.value = get_substring(content, current_index, 1);

                append(tokens, token);
                
                current_index++;

                indentation++; // Cause dashes should be ignored for indentation
                eat_spaces_and_get_indentation(content, current_index, indentation, line);
                continue;
            }

            // Key Value pair colon
            if (content[current_index] == ':' && (current_index + 1 >= content.size || is_white_space(content[current_index + 1])))
            {
                // Previous token is considered a key now
                if (tokens.size > 0 && (tokens[tokens.size - 1].type == Token::Type::SCALAR))
                    tokens[tokens.size - 1].type = Token::Type::KEY;

                force_key = false;
                current_index++;
                continue;
            }

            {   // Scalar string probably!
                u64 str_size = 0;
                u64 actual_size = 0;

                while (true)
                {
                    const u64 index = current_index + actual_size;

                    // Eat till the next new line or EOF
                    if (index >= content.size || content[index] == '\n')
                        break;

                    // Key value pair colon
                    if (content[index] == ':' && (index + 1 >= content.size || is_white_space(content[index + 1])))
                    {
                        force_key = true;
                        break;
                    }

                    if (content[index] == '#' && (index == 0 || is_white_space(content[index - 1])))
                        break;

                    actual_size++;
                    str_size = is_white_space(content[index]) ? str_size : actual_size;
                }

                Token token;
                token.index = current_index;
                token.indentation = indentation;
                token.line = line;
                token.type  = force_key ? Token::Type::KEY : Token::Type::SCALAR;
                token.is_quoted = false;
                token.value = get_substring(content, current_index, str_size);

                append(tokens, token);

                current_index += actual_size;
            }
        }
    }

    #ifdef GN_LOG_SERIALIZATION
        print("LEXER OUTPUT (token count: %)\n", tokens.size);

        for (u64 i = 0; i < tokens.size; i++)
            print("  (%, %), type_id: %, value: '%'\n", tokens[i].indentation, tokens[i].line, get_token_type_name(tokens[i].type), tokens[i].value);
    #endif // GN_LOG_SERIALIZATION


    return !encountered_error;
}

} // namespace Yaml