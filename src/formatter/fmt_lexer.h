#pragma once

#include "core/types.h"
#include "containers/string.h"
#include "containers/darray.h"

namespace Fmt
{

struct Token
{
    enum struct Type : u8
    {
        NONE,

        // Value Types
        RAW_STRING,
        IDENTIFIER,
        STRING,
        INTEGER,
        BOOLEAN,
        TOKENS,

        // Reserved
        IF,
        FOR,
        FILE,
        ELSE,
        
        // Punctuations
        FMT_START,
        FMT_END,
        BRACKET_OPEN  = '[',
        BRACKET_CLOSE = ']',
        COLON         = ':',
        COMMA         = ',',
        DOT           = '.',

        // Operators
        EQUAL         = '=',
        AND           = '&',
        OR            = '|'
    };

    Type type;
    u64 index;

    union
    {
        bool   boolean;
        String string;  // Used by punctuations, strings, and raw strings
        s64    integer;
        DynamicArray<Token> tokens;
    };
};

bool tokenize(const String content, DynamicArray<Token>& tokens);

static inline constexpr String get_token_type_name(Token::Type type)
{
    #define declare_type_name(t) case Token::Type::t: return ref(#t)

    switch (type)
    {
        declare_type_name(NONE);
        
        declare_type_name(RAW_STRING);
        declare_type_name(IDENTIFIER);
        declare_type_name(STRING);
        declare_type_name(INTEGER);
        declare_type_name(BOOLEAN);
        declare_type_name(TOKENS);

        declare_type_name(IF);
        declare_type_name(FOR);
        declare_type_name(FILE);
        declare_type_name(ELSE);

        declare_type_name(FMT_START);
        declare_type_name(FMT_END);
        declare_type_name(BRACKET_OPEN);
        declare_type_name(BRACKET_CLOSE);
        declare_type_name(COLON);
        declare_type_name(COMMA);
        declare_type_name(DOT);
        declare_type_name(EQUAL);
        declare_type_name(AND);
        declare_type_name(OR);
    }

    #undef declare_type_name

    return ref("Error");
}

} // namespace Fmt
