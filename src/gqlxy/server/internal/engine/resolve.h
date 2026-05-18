#pragma once

#include <gqlxy/server/definitions/schema_definition.h>
#include <any>
#include <gqlxy/core/parser/ast/fragments.h>
#include <gqlxy/core/parser/ast/selection_set.h>
#include <gqlxy/core/task.h>
#include <gqlxy/server/schema.h>
#include <nlohmann/json.hpp>
#include <string>
#include <unordered_map>
#include <vector>

namespace gqlxy {
struct ValueResolver;
class ResolverArgs;
struct SchemaDefinition;
}

namespace gqlxy::internal {

struct ResolveQueryArgs {
    std::string query;
    nlohmann::json variables;
    SchemaDefinition schemaDefinition;
    Resolver resolvers;
    Directives directives;
    Scalars scalars;
    std::string operationName;
    std::any context;
};

using Path = std::vector<std::string>;

std::optional<std::string> FieldTypeName(const std::optional<std::string>& typeName,
                                         const std::string& fieldName,
                                         const SchemaDefinition& schemaDefinition);

Task<nlohmann::json> Resolve(const ResolveQueryArgs& args,
                             const ValueResolver& resolver,
                             const ResolverArgs& resolverArgs,
                             const std::optional<parser::SelectionSet>& selectionSet,
                             const std::optional<std::string>& typeName,
                             const parser::Fragments& fragments,
                             GraphQLErrors& GraphQLErrors,
                             const Path& path);

Task<GraphQLResponse> ResolveOperations(const ResolveQueryArgs& args);

}