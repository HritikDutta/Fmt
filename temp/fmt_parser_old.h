#pragma once

#include "containers/string.h"
#include "containers/hash_table.h"
#include "containers/string_builder.h"
#include "serialization/json.h"

#include "variables.h"

namespace Fmt
{

struct Pass
{
    HashTable<String, VariableData> variables;
    StringBuilder builder;
};

void prepare_data(Pass& pass, const Json::Value& base_data);
void prepare_pass(Pass& pass, const Json::Value& params);

String parse_template(const String content, Pass& pass);

} // namespace Fmt