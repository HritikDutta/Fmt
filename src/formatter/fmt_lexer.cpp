#include "fmt_lexer.h"

#include "core/types.h"
#include "containers/string.h"
#include "containers/darray.h"
#include "core/utils.h"

#include "fmt_error.h"
#include "fmt_lexer_debug.h"

namespace Fmt
{

static inline bool is_valid_identifier_char(char ch)
{
    return ch == '-' || ch == '_' || is_alphabet(ch);
}

// Returns true if error is encountered
bool tokenize(const String content, DynamicArray<Token>& tokens, u64& current_index, char end_delim = '}')
{
    clear(tokens);
    resize(tokens, max(2ui64, content.size / 10)); // Just an estimate

    bool encountered_error = false;
    u64 scan_start = current_index;

    while (true)
    {
        // Won't check for null character to end lexing since strings have a specified length

        // Reached EOF
        if (current_index >= content.size)
            break;

        // Check for end delimiter without a backslash
        if (current_index > 0 && (content[current_index - 1] != '\\' && content[current_index] == end_delim))
            break;
        
        if (current_index >= content.size - 1 || content[current_index] != '<' || content[current_index + 1] != '$')
        {
            current_index++;
            continue;
        }

        // Now we're inside a tag
        
        // Append raw string as a token if needed
        if (current_index > scan_start)
        {
            Token token;
            token.type = Token::Type::RAW_STRING;
            token.index = scan_start;
            token.string = get_substring(content, scan_start, current_index - scan_start);
            append(tokens, token);
        }

        {   // Append FMT token
            Token token;
            token.type = Token::Type::FMT_START;
            token.index = current_index;
            append(tokens, token);
        }
    
        current_index += 2;
        bool inside_fmt_tag = true;
        while (inside_fmt_tag)
        {
            if (current_index >= content.size)
            {
                log_error(content, current_index, "Fmt tag was never closed!");
                encountered_error = true;
                break;
            }

            switch (content[current_index])
            {
                // Skip whitespace
                case ' ':  case '\t':
                case '\r': case '\n':
                case '\0':
                {
                    current_index++;
                } break;

                case '$':
                {
                    // Check for end of tag
                    if (content[current_index + 1] == '>')
                    {
                        {   // Append FMT token
                            Token token;
                            token.type = Token::Type::FMT_END;
                            token.index = current_index;
                            append(tokens, token);
                        }

                        current_index += 2;
                        scan_start = current_index;
                        inside_fmt_tag = false;
                    }
                } break;

                case '/':
                {
                    if (content[current_index + 1] != '/')
                    {
                        log_error(content, current_index, "Comment tag needs two '/'s at the start!");
                        encountered_error = true;
                    }

                    current_index += 2;

                    // Ignore the entire tag
                    while (true)
                    {
                        // Reached EOF before closing comment
                        if (current_index >= content.size - 1)
                        {
                            log_error(content, current_index, "Comment tag was not closed!");
                            encountered_error = true;
                            break;
                        }

                        if (content[current_index] == '$' && content[current_index + 1] == '>')
                        {
                            {   // Append FMT token
                                Token token;
                                token.type = Token::Type::FMT_END;
                                token.index = current_index;
                                append(tokens, token);
                            }

                            current_index += 2;
                            scan_start = current_index;
                            inside_fmt_tag = false;
                            break;
                        }
                        
                        current_index++;
                    }
                } break;
                
                // Punctuations and operators
                case (char) Token::Type::BRACKET_OPEN:
                case (char) Token::Type::BRACKET_CLOSE:
                case (char) Token::Type::COLON:
                case (char) Token::Type::COMMA:
                case (char) Token::Type::EQUAL:
                case (char) Token::Type::DOT:
                case (char) Token::Type::AND:
                case (char) Token::Type::OR:
                {
                    Token token;
                    token.index = current_index;
                    token.type  = (Token::Type) content[current_index];
                    token.string = ref(content.data + current_index, 1);

                    append(tokens, token);

                    current_index++;
                } break;
                
                // Braces indicate fmt text inside a fmt tag
                case '{':
                {
                    
                    Token token;
                    token.index = current_index;
                    token.type  = Token::Type::TOKENS;
                    token.tokens = {};

                    current_index++; // Skip brace
                    encountered_error = tokenize(content, token.tokens, current_index);
                    current_index++; // Skip delimitter

                    append(tokens, token);
                } break;

                // String
                case '\"':
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
                            log_error(content, current_index, "String was not closed!");
                            encountered_error = true;
                            break;
                        }

                        if (content[index] == '\"')
                            break;

                        // Reached new line before closing string
                        if (content[index] == '\n')
                        {
                            log_error(content, current_index, "Reached new line before closing string!");
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
                    token.type  = Token::Type::STRING;
                    token.string = get_substring(content, current_index, str_size);

                    append(tokens, token);

                    // Skip the ending "
                    current_index += str_size + 1;
                } break;

                // Number (integer or float)
                case '-':
                case '0': case '1': case '2': case '3': case '4':
                case '5': case '6': case '7': case '8': case '9':
                {
                    u64 number_size = 1;

                    while (true)
                    {
                        const u64 index = current_index + number_size;

                        if (index >= content.size)
                            break;

                        // - is not allowed between numbers (no math allowed!)
                        if (content[index] == '-')
                        {
                            log_error(content, index, "'-' sign can only be used at the start of a number!");
                            encountered_error = true;
                        }
                        
                        if (!is_digit(content[index]))
                            break;

                        number_size++;
                    }
                    
                    Token token;
                    token.index = current_index;
                    token.type  = Token::Type::INTEGER;
                    token.integer = _atoi64(content.data + current_index);

                    append(tokens, token);

                    current_index += number_size;

                } break;

                // Probably an identifier
                default:
                {
                    // Identifiers can only start with alphabets, dashes, and underscores
                    if (!is_valid_identifier_char(content[current_index]))
                    {
                        log_error(content, current_index, "Encountered invalid token! (found token: '%')");
                        encountered_error = true;
                        current_index++;
                        break;
                    }

                    u64 identifier_size = 1;
                    
                    while (true)
                    {
                        const u64 index = current_index + identifier_size;

                        // Identifiers can have digits but not as their first character
                        if (index >= content.size || (!is_valid_identifier_char(content[index]) && !is_digit(content[index])))
                            break;

                        identifier_size++;
                    }
                    
                    Token token;
                    token.index = current_index;
                    token.type  = Token::Type::IDENTIFIER;
                    token.string = get_substring(content, current_index, identifier_size);

                    if (token.string == ref("if"))
                        token.type = Token::Type::IF;
                    else if (token.string == ref("for"))
                        token.type = Token::Type::FOR;
                    else if (token.string == ref("file"))
                        token.type = Token::Type::FILE;
                    else if (token.string == ref("else"))
                        token.type = Token::Type::ELSE;
                    else if (token.string == ref("true"))
                    {
                        token.boolean = true;
                        token.type = Token::Type::BOOLEAN;
                    }
                    else if (token.string == ref("false"))
                    {
                        token.boolean = false;
                        token.type = Token::Type::BOOLEAN;
                    }

                    append(tokens, token);

                    current_index += identifier_size;
                } break;
            }
        }
    }
    
    // Append remaining raw string if needed
    if (current_index > scan_start)
    {
        Token token;
        token.type = Token::Type::RAW_STRING;
        token.index = scan_start;
        token.string = get_substring(content, scan_start, current_index - scan_start);
        append(tokens, token);
    }

    return encountered_error;
}

bool tokenize(const String content, DynamicArray<Token> &tokens)
{
    u64 current_index = 0;
    bool error = tokenize(content, tokens, current_index, '\0');

    // for (u64 i = 0; i < tokens.size; i++)
    //     print_token(0, tokens[i]);
        
    return !error;
}

} // namespace Fmt