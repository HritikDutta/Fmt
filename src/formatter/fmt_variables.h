#pragma once

#include "core/types.h"
#include "containers/string.h"
#include "containers/darray.h"
#include "containers/hash_table.h"

#include "fmt_lexer.h"

namespace Fmt
{

struct VariableData
{
    enum struct Type : u8
    {
        NONE,

        BOOLEAN,
        INTEGER,
        STRING,
        RANGE,
        ARRAY,
        OBJECT,

        TOKENS,

        NUM_TYPES
    };

    Type type;
    union
    {
        bool   boolean;
        s64    integer;
        String string;
        struct { s64 start, end; } range;
        DynamicArray<VariableData> array;
        HashTable<String, VariableData> object;
        DynamicArray<Token> tokens;
    };
};

static inline constexpr String get_variable_type_name(VariableData::Type type)
{
    #define declare_type_name(t) case VariableData::Type::t: return ref(#t)

    switch (type)
    {
        declare_type_name(NONE);
        declare_type_name(BOOLEAN);
        declare_type_name(INTEGER);
        declare_type_name(STRING);
        declare_type_name(RANGE);
        declare_type_name(ARRAY);
        declare_type_name(OBJECT);
        declare_type_name(TOKENS);
    }

    #undef declare_type_name

    return ref("Error");
}

static inline bool variable_equal(const VariableData& var1, const VariableData& var2)
{
    if (var1.type != var2.type)
        return false;

    switch (var1.type)
    {
        case VariableData::Type::NONE:
            return true;    // Nothing to check here!

        case VariableData::Type::BOOLEAN:
            return var1.boolean == var2.boolean;
            
        case VariableData::Type::INTEGER:
            return var1.integer == var2.integer;
            
        case VariableData::Type::STRING:
            return var1.string == var2.string;
        
        case VariableData::Type::RANGE:
            return var1.range.start == var2.range.start && var1.range.end == var2.range.end;

        case VariableData::Type::ARRAY:
        {
            if (var1.array.size != var2.array.size)
                return false;
            
            for (u64 i = 0; i < var1.array.size; i++)
            {
                if (!variable_equal(var1.array[i], var2.array[i]))
                    return false;
            }

            return true;
        }

        case VariableData::Type::TOKENS:
            // Just check whether both variables point to the same tokens array
            return var1.tokens.data == var2.tokens.data && var1.tokens.size == var2.tokens.size;
    }

    // It's not ideal
    // Since The hash table is the biggest element here, it should be okay to just compare all the bytes
    // I'll handle this later if it causes any bugs
    return platform_compare_memory(&var1, &var2, sizeof(VariableData));
}

} // namespace Fmt