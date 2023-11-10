#pragma once

typedef signed char        s8;
typedef short              s16;
typedef int                s32;
typedef long long          s64;
typedef unsigned char      u8;
typedef unsigned short     u16;
typedef unsigned int       u32;
typedef unsigned long long u64;

typedef float  f32;
typedef double f64;

namespace CompileTimeChecks
{

template <typename T> constexpr inline bool is_type_integral() { return false; }

#define DECLARE_TYPE_INTEGRAL(type) \
template <> constexpr inline bool is_type_integral<type>() { return true; }

DECLARE_TYPE_INTEGRAL(s8);
DECLARE_TYPE_INTEGRAL(s16);
DECLARE_TYPE_INTEGRAL(s32);
DECLARE_TYPE_INTEGRAL(s64);
DECLARE_TYPE_INTEGRAL(u8);
DECLARE_TYPE_INTEGRAL(u16);
DECLARE_TYPE_INTEGRAL(u32);
DECLARE_TYPE_INTEGRAL(u64);

#undef DECLARE_TYPE_INTEGRAL

}

#define is_type_integral(type) CompileTimeChecks::is_type_integral<type>()