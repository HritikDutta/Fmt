#pragma once

#include "core/types.h"
#include "containers/string.h"

namespace Fmt
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

} // namespace Fmt

#ifdef GN_DEBUG
#include "core/logger.h"
#define log_error(content, index, fmt, ...) { u64 line, col; line_number(content, index, line, col); print_error("Fmt Error[%, %]: " fmt "\n", line, col, __VA_ARGS__); gn_break_point(); }
#else
#define log_error(content, index, fmt, ...)
#endif // GN_DEBUG