#pragma once

#include <ariane/internal/ast/Fragments.h>
#include <ariane/internal/ast/SelectionSet.h>
#include <ariane/internal/introspection/types/SchemaDefinition.h>
#include <ariane/schema.h>
#include <ariane/task.h>
#include <any>
#include <nlohmann/json.hpp>
#include <string>
#include <unordered_map>
#include <vector>

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