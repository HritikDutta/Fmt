#pragma once

#include "containers/bytes.h"
#include "serialization/slz.h"

namespace Binary
{

Bytes json_document_to_binary(const Slz::Document& document);

} // namespace Binary