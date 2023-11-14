#pragma once

#include "core/types.h"
#include "containers/string.h"
#include "containers/darray.h"
#include "containers/hash_table.h"

#include "fmt_lexer.h"

namespace Fmt
{

struct TokenList
{
    bool encountered_error;
    String content;
    DynamicArray<Token> tokens;
};

struct Variable;

struct VariableData
{
    enum struct Type : u8
    {
        NONE,

        BOOLEAN,
        INTEGER,
        STRING,
        ARRAY,
        OBJECT,

        TOKEN_LIST,

        NUM_TYPES
    };

    Type type;
    union
    {
        bool   boolean;
        s64    integer;
        String string;
        DynamicArray<VariableData> array;
        HashTable<String, Variable> object;
        TokenList token_list;
    };
};

struct Variable
{
    String name;
    VariableData data;
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
        declare_type_name(ARRAY);
        declare_type_name(OBJECT);
        declare_type_name(TOKEN_LIST);
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

        case VariableData::Type::TOKEN_LIST:
            // Just check whether both variables point to the same tokens array
            return var1.token_list.tokens.data == var2.token_list.tokens.data &&
                   var1.token_list.tokens.size == var2.token_list.tokens.size;
    }

    // It's not ideal
    // Since The hash table is the biggest element here, it should be okay to just compare all the bytes
    // I'll handle this later if it causes any bugs
    return platform_compare_memory(&var1, &var2, sizeof(VariableData));
}

} // namespace Fmt