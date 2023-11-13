#pragma once

#ifdef GN_LOG_SERIALIZATION

#include "core/types.h"
#include "core/logger.h"
#include "slz_document.h"

namespace Slz
{

static u64 print_node_info(const Document& document, u64 index, u64 indent)
{

    const DependencyNode& node = document.dependency_tree[index];

    char spaces[] = "                                                                ";
    switch (node.type)
    {
        case Type::STRING:
        {
            const Resource& res = document.resources[node.index];
            print("%String \"%\"\n", ref(spaces, indent), res.string);
        } break;
        
        case Type::INTEGER:
        {
            const Resource& res = document.resources[node.index];
            print("%Integer '%'\n", ref(spaces, indent), res.integer64);
        } break;
        
        case Type::FLOAT:
        {
            const Resource& res = document.resources[node.index];
            print("%Float '%'\n", ref(spaces, indent), res.float64);
        } break;
        
        case Type::BOOLEAN:
        {
            const Resource& res = document.resources[node.index];
            print("%Boolean '%'\n", ref(spaces, indent), res.boolean);
        } break;
        
        case Type::NONE:
        {
            print("%null\n", ref(spaces, indent));
        } break;

        case Type::ARRAY:
        {
            print("%::Array Start::\n", ref(spaces, indent));

            for (u64 i = 0; i < node.array.size; i++)
                index = print_node_info(document, node.array[i], indent + 2);

            print("%::Array End::\n", ref(spaces, indent));
        } break;

        case Type::OBJECT:
        {
            print("%::Object Start::\n", ref(spaces, indent));

            for (u64 i = 0; i < node.object.capacity; i++)
            {
                if (node.object.states[i] == ObjectNode::State::ALIVE)
                {
                    print("%%:\n", ref(spaces, indent + 2), node.object.keys[i]);
                    index = print_node_info(document, node.object.values[i], indent + 4);
                }
            }

            print("%::Object End::\n", ref(spaces, indent));
        } break;
    }

    return index + 1;
}

static void document_debug_output(const Document& document)
{
    print("PARSER OUTPUT (tree size: %)\n", document.dependency_tree.size);
    print_node_info(document, 3, 0);
}

} // namespace SLZ

#endif // GN_LOG_SERIALIZATION