#pragma once

#include <gqlxy/internal/ast/Fragments.h>
#include <gqlxy/internal/ast/SelectionSet.h>
#include <gqlxy/internal/introspection/types/SchemaDefinition.h>
#include <gqlxy/schema.h>
#include <gqlxy/task.h>
#include <any>
#include <nlohmann/json.hpp>
#include <string>
#include <unordered_map>
#include <vector>

namespace gqlxy {
struct ValueResolver;
class ResolverArgs;
}

namespace gqlxy::internal {
struct SchemaDefinition;

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
                             const std::optional<SelectionSet>& selectionSet,
                             const std::optional<std::string>& typeName,
                             const Fragments& fragments,
                             FieldErrors& fieldErrors,
                             const Path& path);

Task<ResolveResult> ResolveOperations(const ResolveQueryArgs& args);

}