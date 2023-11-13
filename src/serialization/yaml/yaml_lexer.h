#pragma once

#include "core/types.h"
#include "containers/darray.h"
#include "containers/string.h"

namespace Yaml
{

struct Token
{
    enum struct Type: char
    {
        // Literals (multiple characters)
        KEY,
        SCALAR,

        // Punctuations (single character)
        BRACKET_OPEN  = '[',
        BRACKET_CLOSE = ']',
        BRACE_OPEN    = '{',
        BRACE_CLOSE   = '}',
        COMMA         = ',',
        DASH          = '-',

        LITERAL_BLOCK_SCALAR = '|',
        FOLDER_BLOCK_SCALAR  = '>',
    };

    Type type;
    u64 index;
    s64 line;
    s64 indentation;
    String value; // Not owned
    bool is_quoted;
};

bool tokenize(const String content, DynamicArray<Token>& tokens);

static inline constexpr String get_token_type_name(Token::Type type)
{
    #define declare_type_name(t) case Token::Type::t: return ref(#t)

    switch (type)
    {
        declare_type_name(KEY);
        declare_type_name(SCALAR);

        declare_type_name(BRACKET_OPEN);
        declare_type_name(BRACKET_CLOSE);
        declare_type_name(BRACE_OPEN);
        declare_type_name(BRACE_CLOSE);
        declare_type_name(COMMA);
        declare_type_name(DASH);

        declare_type_name(LITERAL_BLOCK_SCALAR);
        declare_type_name(FOLDER_BLOCK_SCALAR);
    }

    #undef declare_type_name
    return ref("Error");
}

} // namespace Yaml
