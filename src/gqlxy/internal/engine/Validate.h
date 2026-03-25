#pragma once

#include <gqlxy/schema.h>
#include <nlohmann/json_fwd.hpp>

namespace gqlxy::internal {
struct SchemaDefinition;
struct Document;

FieldErrors ValidateDocument(const Document& document, const SchemaDefinition& schema, const nlohmann::json& variables);

}
