#pragma once

#include <ariane/schema.h>
#include <nlohmann/json_fwd.hpp>

namespace ariane::graphql::internal {
struct SchemaDefinition;
struct Document;

FieldErrors ValidateDocument(const Document& document, const SchemaDefinition& schema, const nlohmann::json& variables);

}
