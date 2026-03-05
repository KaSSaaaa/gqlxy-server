#pragma once

#include <ariane/resolvers.h>

#include <memory>
#include <string>
#include <unordered_map>

namespace ariane::graphql {

namespace internal {
struct SchemaDefinition;
}

struct SchemaOptions {
    std::string typeDefs;
    Resolver resolvers;
    Directives directives;
    bool allowIntrospection = true;
};

struct ErrorLocation {
    int line;
    int column;
};

struct FieldError {
    std::string message;
    std::vector<std::string> path;
    std::vector<ErrorLocation> locations;
};

using FieldErrors = std::vector<FieldError>;

struct ResolveResult {
    std::optional<std::string> data;
    std::optional<FieldErrors> errors;
};

struct SchemaResolveArgs {
    std::string query;
    nlohmann::json variables = nlohmann::json::object();
};

class Schema {
public:
    explicit Schema(const SchemaOptions& options);

    Task<ResolveResult> Resolve(const SchemaResolveArgs& args) const;

private:
    std::shared_ptr<internal::SchemaDefinition> _schemaDefinition;
    Resolver _resolvers;
    Directives _directives;

    void InjectIntrospectionResolvers();
    void AddToResolver(const std::string& resolverName, const Resolver& resolver);
};

}