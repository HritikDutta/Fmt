#pragma once

#include "core/types.h"
#include "fmt_lexer.h"

namespace Fmt
{

static inline void print_token(int indent, const Token& token)
{
    constexpr int tab_size = 2;
    char spaces[] = "                                                               ";
    switch (token.type)
    {
        case Token::Type::RAW_STRING:
        {
            print("%%: '%'\n\n", ref(spaces, indent), get_token_type_name(token.type), token.string);
        } break;

        case Token::Type::STRING:
        case Token::Type::IDENTIFIER:
        {
            print("%%: '%'\n", ref(spaces, indent), get_token_type_name(token.type), token.string);
        } break;

        case Token::Type::BOOLEAN:
        {
            print("%%: '%'\n", ref(spaces, indent), get_token_type_name(token.type), token.boolean ? ref("true") : ref("false"));
        } break;

        case Token::Type::INTEGER:
        {
            print("%%: '%'\n", ref(spaces, indent), get_token_type_name(token.type), token.integer);
        } break;

        case Token::Type::TOKENS:
        {
            print("%%\n", ref(spaces, indent), get_token_type_name(token.type));
            for (u64 i = 0; i < token.tokens.size; i++)
                print_token(indent + tab_size, token.tokens[i]);
            print("%END\n", ref(spaces, indent));
        } break;

        default:
        {
            print("%%\n", ref(spaces, indent), get_token_type_name(token.type));
            
            if (token.type == Token::Type::FMT_END)
                print("\n");
        } break;
    }
}

} // namespace Fmt
