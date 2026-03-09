#pragma once

#include <ariane/resolvers.h>

#include <any>
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
    Scalars scalars;
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

template<typename TContext = std::monostate>
struct SchemaResolveArgs {
    std::string query;
    nlohmann::json variables = nlohmann::json::object();
    std::string operationName;
    TContext context = {};
};

class Schema {
public:
    explicit Schema(const SchemaOptions& options);

    template<typename TContext = std::monostate>
    Task<ResolveResult> Resolve(const SchemaResolveArgs<TContext>& args) const {
        return ResolveInternal(args.query, args.variables, args.operationName, args.context);
    }

private:
    std::shared_ptr<internal::SchemaDefinition> _schemaDefinition;
    Resolver _resolvers;
    Directives _directives;
    Scalars _scalars;

    Task<ResolveResult> ResolveInternal(const std::string& query,
                                        const nlohmann::json& variables,
                                        const std::string& operationName,
                                        std::any context) const;

    void InjectIntrospectionResolvers();
    void AddToResolver(const std::string& resolverName, const Resolver& resolver);
};

}