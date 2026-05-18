#pragma once

#include <gqlxy/core/parser/ast/document.h>
#include <gqlxy/server/schema.h>
#include <nlohmann/json_fwd.hpp>

namespace gqlxy::internal {

GraphQLErrors ValidateDocument(const parser::Document& document, const SchemaDefinition& schema, const nlohmann::json& variables);

}
