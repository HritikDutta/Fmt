#pragma once

#include "yaml_lexer.h"
#include "serialization/slz.h"

namespace Yaml
{

bool parse_tokens(const DynamicArray<Token>& tokens, const String content, Slz::Document& out);
bool parse_string(const String content, Slz::Document& out);

} // namespace Yaml