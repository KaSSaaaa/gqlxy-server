#pragma once
#include <ariane/internal/ast/Document.h>
#include <ariane/internal/introspection/types/SchemaDefinition.h>
#include <ariane/schema.h>
#include <ariane/task.h>
#include <nlohmann/json_fwd.hpp>
#include <string>
#include <unordered_map>

namespace ariane::graphql {
struct ValueResolver;
struct ResolverArgs;
}

namespace ariane::graphql::internal {
struct SchemaDefinition;

struct ResolveQueryArgs {
  std::string query;
  nlohmann::json variables;
  SchemaDefinition schemaDefinition;
  Resolver resolvers;
  Directives directives;
  Scalars scalars;
};

Task<ResolveResult> ResolveOperations(ResolveQueryArgs args);

}