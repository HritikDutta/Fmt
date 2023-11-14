#pragma once

#pragma once

#include "containers/string.h"
#include "containers/hash_table.h"
#include "containers/string_builder.h"
#include "serialization/json.h"

#include "fmt_variables.h"
#include "fmt_lexer.h"

namespace Fmt
{

struct Pass
{
    Variable root_var;
    DynamicArray<Token> tokens;
};

void prepare_data(Pass& pass, const Slz::Value& base_data);
void prepare_pass(Pass& pass, const Slz::Value& params);

bool parse_template(const String content, Pass& pass, StringBuilder& builder);

} // namespace Fmt