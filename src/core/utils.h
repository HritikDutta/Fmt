#pragma once

#include <cstdlib>
#include "core/types.h"

struct String;

// Signed
void to_string(String& str, s32 integer, u32 radix = 10);
void to_string(String& str, s64 integer, u32 radix = 10);

// Unsigned
void to_string(String& str, u32 integer, u32 radix = 10);
void to_string(String& str, u64 integer, u32 radix = 10);

// Floats
void to_string(String& str, f32 number, u32 after_decimal = 4);
void to_string(String& str, f64 number, u32 after_decimal = 4);

inline bool is_alphabet(char ch)
{
    return ((ch >= 'a') && (ch <= 'z')) ||
           ((ch >= 'A') && (ch <= 'Z'));
}

inline static bool is_digit(char ch)
{
    return (ch >= '0') && (ch <= '9');
}

inline static bool is_white_space(char ch)
{
    return ch == ' '  || ch == '\t' || ch == '\r' || ch == '\n';
}