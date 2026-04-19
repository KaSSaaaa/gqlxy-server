#pragma once

#include <gqlxy/parser/ast/document.h>
#include <gqlxy/schema.h>
#include <nlohmann/json_fwd.hpp>

namespace gqlxy::internal {
struct SchemaDefinition;

FieldErrors ValidateDocument(const parser::Document& document, const SchemaDefinition& schema, const nlohmann::json& variables);

}
