#include "fmt_parser.h"

#include "containers/string.h"
#include "containers/hash_table.h"
#include "containers/string_builder.h"
#include "serialization/slz.h"

#include "fmt_variables.h"

namespace Fmt
{

static VariableData extract_data_from_json_value(const Slz::Value& value)
{
    VariableData var;

    switch (value.type())
    {
        case Slz::Type::BOOLEAN:
        {
            var.type = VariableData::Type::BOOLEAN;
            var.boolean = value.boolean();
        } break;

        case Slz::Type::INTEGER:
        {
            var.type = VariableData::Type::INTEGER;
            var.integer = value.int64();
        } break;

        case Slz::Type::STRING:
        {
            var.type = VariableData::Type::STRING;
            var.string = value.string();
        } break;

        case Slz::Type::ARRAY:
        {
            const Slz::Array& json_array = value.array();
            
            var.type = VariableData::Type::ARRAY;
            var.array = make<DynamicArray<VariableData>>(json_array.size());

            for (u64 i = 0; i < json_array.size(); i++)
                append(var.array, extract_data_from_json_value(json_array[i]));
        } break;

        case Slz::Type::OBJECT:
        {
            const Slz::Object& json_object = value.object();

            var.type = VariableData::Type::OBJECT;
            var.object = make<HashTable<String, Variable>>(json_object.capacity());

            const auto& json_object_internal_ht = json_object.document->dependency_tree[json_object.tree_index].object;
            u64 filled = json_object_internal_ht.filled;
            for (u64 i = 0; filled > 0 && i < json_object_internal_ht.capacity; i++)
            {
                if (json_object_internal_ht.states[i] == Slz::ObjectNode::State::ALIVE)
                {
                    Slz::Value prop_value = { json_object.document, json_object_internal_ht.values[i] };
                    Variable child;
                    child.name = json_object_internal_ht.keys[i];
                    child.data = extract_data_from_json_value(prop_value);

                    put(var.object, json_object_internal_ht.keys[i], child);
                    filled--;
                }
            }
        } break;
    }

    return var;
}

void prepare_data(Pass& pass, const Slz::Value& base_data)
{
    pass.root_var.name = ref("root");
    VariableData& root_var_data = pass.root_var.data;

    root_var_data.type = VariableData::Type::OBJECT;

    const Slz::Object& json_object = base_data.object();
    root_var_data.object = make<HashTable<String, Variable>>(json_object.capacity());

    const auto& json_object_internal_ht = json_object.document->dependency_tree[json_object.tree_index].object;
    u64 filled = json_object_internal_ht.filled;
    for (u64 i = 0; filled > 0 && i < json_object_internal_ht.capacity; i++)
    {
        if (json_object_internal_ht.states[i] == Slz::ObjectNode::State::ALIVE)
        {
            // Add all variables other than templates (Maybe something I can add later?)
            if (json_object_internal_ht.keys[i] == ref("templates"))
                continue;

            const Slz::Value prop_value = { json_object.document, json_object_internal_ht.values[i] };

            Variable child;
            child.name = json_object_internal_ht.keys[i];
            child.data = extract_data_from_json_value(prop_value);
            put(root_var_data.object, json_object_internal_ht.keys[i], child);
            filled--;
        }
    }
}

void prepare_pass(Pass& pass, const Slz::Value& params)
{
    const String params_string = ref("params");

    auto& result = find(pass.root_var.data.object, params_string);

    Variable params_var;

    if (result)
    {
        params_var = result.value();

        // Discard previous values for params
        if (params_var.data.type == VariableData::Type::OBJECT)
            free(params_var.data.object);
        else if (params_var.data.type == VariableData::Type::ARRAY)
            free(params_var.data.array);
    }
    else
    {
        params_var.name = params_string;
    }

    params_var.data = extract_data_from_json_value(params);
    put(pass.root_var.data.object, params_string, params_var);
}

}