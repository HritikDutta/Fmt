#include "fmt_parser_old.h"

#include "containers/string.h"
#include "containers/string_builder.h"
#include "containers/darray.h"
#include "core/utils.h"
#include "fileio/fileio.h"
#include "serialization/json.h"

#include "variables.h"
#include "fmt_error.h"
#include "fmt_lexer.h"

namespace Fmt
{
    
struct ParseContext
{    
    bool encountered_error;
    u64 current_index;
    HashTable<String, VariableData> variable_members;
};

static inline bool is_alphabet(char ch)
{
    return ((ch >= 'a') && (ch <= 'z')) ||
           ((ch >= 'A') && (ch <= 'Z'));
}

static inline bool is_identifier_char(char ch)
{
    return ch == '-' || ch == '_' || is_alphabet(ch);
}

static inline bool is_digit(char ch)
{
    return (ch >= '0') && (ch <= '9');
}

static inline void eat_spaces(const String content, ParseContext& ctx)
{
    while (ctx.current_index < content.size &&
           (content[ctx.current_index] == ' '  ||
            content[ctx.current_index] == '\t' ||
            content[ctx.current_index] == '\r' ||
            content[ctx.current_index] == '\n'))
    {
        ctx.current_index++;
    }
}

static inline String get_identifier(const String content, ParseContext& ctx)
{
    // Identifiers can only have alphabets, dashes, and underscores
    if (!is_identifier_char(content[ctx.current_index]))
    {
        log_error("Encountered invalid token! (found token: '%', line: %)", content[ctx.current_index], line_number(content, ctx.current_index));
        ctx.encountered_error = true;
        ctx.current_index++;
        return ref("");
    }

    u64 identifier_size = 1;
    
    while (true)
    {
        const u64 index = ctx.current_index + identifier_size;

        if (index >= content.size || !is_identifier_char(content[index]))
            break;

        identifier_size++;
    }

    String identifier = get_substring(content, ctx.current_index, identifier_size);
    ctx.current_index += identifier_size;

    return identifier;
}

static void parse_content(const String content, Pass& pass, ParseContext& ctx);

static VariableData parse_variable(const String identifier, const String content, Pass& pass, ParseContext& ctx)
{
    const auto& var_element = find(ctx.variable_members, identifier);

    if (var_element)
    {
        VariableData& var = var_element.value();
        switch (var.type)
        {
            case VariableData::Type::BOOLEAN:
            case VariableData::Type::STRING:
            case VariableData::Type::INTEGER:
            case VariableData::Type::TOKENS:
            {
                return var;
            }

            case VariableData::Type::ARRAY:
            {
                // TODO: Handle indexing
                return var;
            }

            case VariableData::Type::OBJECT:
            {
                if (content[ctx.current_index] == '.')
                {
                    ctx.current_index++;

                    String member = get_identifier(content, ctx);
                    auto prev_var_members = ctx.variable_members;

                    ctx.variable_members = var.object;
                    VariableData child_var = parse_variable(member, content, pass, ctx);

                    // Reset context
                    ctx.variable_members = prev_var_members;
                    return child_var;
                }

                return var;
            }
        }
    }

    // Error type
    VariableData var;
    var.type = VariableData::Type::NONE;
    return var;
}

static Token get_next_token(const String& content, Pass& pass, ParseContext& ctx)
{
    eat_spaces(content, ctx);

    Token token;
    token.type = Token::Type::NONE;

    switch (content[ctx.current_index])
    {
        case '\"':
        {
            // Skip the first "
            ctx.current_index++;

            // Eat till the next "
            u64 str_size = 0;
            while (true)
            {
                const u64 index = ctx.current_index + str_size;

                // Reached EOF before closing string
                if (index >= content.size)
                {
                    log_error("String was not closed! (line: %)", line_number(content, ctx.current_index));
                    ctx.encountered_error = true;
                    break;
                }

                if (content[index] == '\"')
                    break;

                // Reached new line before closing string
                if (content[index] == '\n')
                {
                    log_error("Reached new line before closing string! (line: %)", line_number(content, index));
                    ctx.encountered_error = true;
                    break;
                }

                // Skip the next character if current character is a '\'
                if (content[index] == '\\')
                    str_size++;

                str_size++;
            }

            token.type = Token::Type::STRING;
            token.content = get_substring(content, ctx.current_index, str_size);
            token.index = ctx.current_index;

            ctx.current_index += str_size + 1;
        } break;

        default:
        {
            token.type = Token::Type::IDENTIFIER;
            token.index = ctx.current_index;
            token.content = get_identifier(content, ctx);

            if (token.content == ref("if"))
                token.type = Token::Type::IF;
            else if (token.content == ref("for"))
                token.type = Token::Type::FOR;
            else if (token.content == ref("file"))
                token.type = Token::Type::FILE;
        } break;
    }

    return token;
}

static void handle_variable(VariableData& var, const String content, Pass& pass, ParseContext& ctx)
{
    eat_spaces(content, ctx);

    switch (content[ctx.current_index])
    {
        case ',':
        {
        } break;

        case ':':
        {
        } break;

        case '$':
        {
            switch (var.type)
            {
                case VariableData::Type::BOOLEAN:
                {
                    append(pass.builder, var.boolean ? ref("true") : ref("false"));
                } break;

                case VariableData::Type::INTEGER:
                {
                    String number_string = make<String>("12312312313123123");
                    to_string(number_string, var.integer);
                    append(pass.builder, number_string);
                } break;

                case VariableData::Type::STRING:
                {
                    append(pass.builder, var.string);
                } break;

                case VariableData::Type::TOKENS:
                {
                    ParseContext var_ctx = {};
                    parse_content(var.string, pass, var_ctx);
                } break;

                default:
                {
                    log_error("Can't convert variable of % type to string for formatting!", get_variable_type_name(var.type));
                    ctx.encountered_error = true;
                } break;
            }
        } break;
    }
}

static void parse_conditional(const String content, Pass& pass, ParseContext& ctx)
{

}

static void parse_loop(const String content, Pass& pass, ParseContext& ctx)
{
    
}

static void parse_file(const String content, Pass& pass, ParseContext& ctx)
{
    // Parse file
    char buffer[256] = {};
    String path = ref(buffer, 0);

    eat_spaces(content, ctx);

    // Check next token
    if (content[ctx.current_index] == '\"')
    {
        // Skip the first "
        ctx.current_index++;

        // Eat till the next "
        u64 str_size = 0;
        while (true)
        {
            const u64 index = ctx.current_index + str_size;

            // Reached EOF before closing string
            if (index >= content.size)
            {
                log_error("String was not closed! (line: %)", line_number(content, ctx.current_index));
                ctx.encountered_error = true;
                break;
            }

            if (content[index] == '\"')
                break;

            // Reached new line before closing string
            if (content[index] == '\n')
            {
                log_error("Reached new line before closing string! (line: %)", line_number(content, index));
                ctx.encountered_error = true;
                break;
            }

            // Skip the next character if current character is a '\'
            if (content[index] == '\\')
                str_size++;

            str_size++;
        }

        // Copy string
        string_copy_into(path, get_substring(content, ctx.current_index, str_size));
        ctx.current_index += str_size + 1;
    }
    else
    {
        // Check for variable
        String identifier = get_identifier(content, ctx);
        VariableData var = parse_variable(identifier, content, pass, ctx);

        if (var.type != VariableData::Type::STRING)
        {
            log_error("Input to file tag should be a variable that has a string! (variable: '', data type: %)", identifier, get_variable_type_name(var.type));
            ctx.encountered_error = true;
            return;
        }

        path = var.string;
    }

    if (ctx.encountered_error)
        return;

    String file_content;

    // If the file was already loaded, then no need to load again
    auto file_content_loaded = find(pass.variables, path);
    if (file_content_loaded)
    {
        gn_assert_with_message(file_content_loaded.value().type == VariableData::Type::TOKENS, "File contents weren't loaded as a string! (path: '%')", path);
        file_content = file_content_loaded.value().string;
    }
    else
    {
        file_content = file_load_string(path);

        VariableData var;
        var.type = VariableData::Type::TOKENS;
        var.string = file_content;

        put(pass.variables, path, var);
    }

    ParseContext file_ctx = {};
    parse_content(file_content, pass, file_ctx);
}

static void parse_content(const String content, Pass& pass, ParseContext& ctx)
{
    u64 scan_start = 0;
    ctx.variable_members = pass.variables;

    while (true)
    {
        // Won't check for null character to end lexing since strings have a specified length
        
        // Reached EOF
        if (ctx.current_index >= content.size)
            break;

        if (ctx.current_index < content.size - 1 && (content[ctx.current_index] == '<' && content[ctx.current_index + 1] == '$'))
        {
            {   // Add the raw string as a token
                append(pass.builder, get_substring(content, scan_start, ctx.current_index - scan_start));
            }

            {   // Parse the fmt tag
                ctx.current_index += 2;  // Offset to inside the fmt tag
                bool inside_fmt_tag = true;

                while (inside_fmt_tag)
                {
                    if (ctx.current_index >= content.size)
                        break;
                        
                    switch (content[ctx.current_index])
                    {
                        // Skip whitespace
                        case ' ':  case '\t':
                        case '\r': case '\n':
                        case '\0':
                        {
                            ctx.current_index++;
                        } break;

                        case '$':
                        {
                            // Check for end of tag
                            if (content[ctx.current_index + 1] == '>')
                            {
                                ctx.current_index += 2;
                                scan_start = ctx.current_index;
                                inside_fmt_tag = false;
                            }
                        } break;

                        case '/':
                        {
                            if (content[ctx.current_index + 1] == '/')
                            {
                                ctx.current_index += 2;

                                // Ignore the entire tag
                                while (true)
                                {
                                    // Reached EOF before closing comment
                                    if (ctx.current_index >= content.size - 1)
                                    {
                                        log_error("Comment tag was not closed! (line: %)", line_number(content, ctx.current_index));
                                        ctx.encountered_error = true;
                                        break;
                                    }

                                    if (content[ctx.current_index] == '$' && content[ctx.current_index + 1] == '>')
                                    {
                                        ctx.current_index += 2;
                                        scan_start = ctx.current_index;
                                        inside_fmt_tag = false;
                                        break;
                                    }
                                    
                                    ctx.current_index++;
                                }
                            }
                        } break;

                        default:
                        {
                            String identifier = get_identifier(content, ctx);

                            if (identifier == ref("if"))
                            {
                                parse_conditional(content, pass, ctx);
                            }
                            else if (identifier == ref("for"))
                            {
                                parse_loop(content, pass, ctx);
                            }
                            else if (identifier == ref("file"))
                            {
                                parse_file(content, pass, ctx);
                            }
                            else
                            {
                                VariableData var = parse_variable(identifier, content, pass, ctx);
                                handle_variable(var, content, pass, ctx);
                            }
                        } break;
                    }
                }
            }
        }

        ctx.current_index++;
    }
    
    {   // Add remaining raw string at the end
        append(pass.builder, get_substring(content, scan_start));
    }
}

String parse_template(const String content, Pass& pass)
{
    clear(pass.builder);

    if (pass.builder.capacity <= 2)
        resize(pass.builder, max(2ui64, content.size / 10)); // Just an estimate

    u64 scan_start = 0;
    ParseContext ctx = {};

    parse_content(content, pass, ctx);

    String result = build_string(pass.builder);
    free(pass.builder);

    return result;
}

} // namespace Fmt