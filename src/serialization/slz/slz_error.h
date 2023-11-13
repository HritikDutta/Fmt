#pragma once

#include "core/types.h"
#include "containers/string.h"

namespace Slz
{

inline void line_number(const String str, u64 index, u64& out_line, u64& out_column)
{
    out_line = 1;
    out_column = index + 1;

    for (u64 i = 0; i < index; i++)
    {
        if (str[i] == '\n')
        {
            out_line++;
            out_column = index - i;
        }
    }
}

} // namespace Slz

#ifdef GN_DEBUG
#include "core/logger.h"

#ifndef SLZ_ERROR_PREFIX
#define SLZ_ERROR_PREFIX "Slz"
#endif

#define log_error(content, index, fmt, ...) { u64 line, col; Slz::line_number(content, index, line, col); print_error(SLZ_ERROR_PREFIX" Error[%, %]: " fmt "\n", line, col, __VA_ARGS__); gn_break_point(); }
#else
#define log_error(content, index, fmt, ...)
#endif // GN_DEBUG