#pragma once

#include <any>
#include <gqlxy/server/resolvers.h>
#include <gqlxy/core/results.h>
#include <gqlxy/server/subscription.h>
#include <memory>
#include <string>
#include <unordered_map>

namespace gqlxy {
struct SchemaDefinition;

namespace internal {
struct ResolveQueryArgs;
}

struct SchemaOptions {
    std::string typeDefs;
    Resolver resolvers;
    Directives directives = {};
    Scalars scalars = {};
    bool allowIntrospection = true;
    bool federation = false;
};

template <typename TContext = std::monostate>
struct SchemaResolveArgs {
    std::string query;
    nlohmann::json variables = nlohmann::json::object();
    std::string operationName = "";
    TContext context = {};
};

class Schema {
public:
    explicit Schema(const SchemaOptions& options);

    template <typename TContext = std::monostate>
    Task<GraphQLResponse> Resolve(const SchemaResolveArgs<TContext>& args) const {
        return ResolveInternal(args.query, args.variables, args.operationName, args.context);
    }

    template <typename TContext = std::monostate>
    SubscriptionHandle Subscribe(const SchemaResolveArgs<TContext>& args) const {
        return SubscribeInternal(args.query, args.variables, args.operationName, args.context);
    }

    Schema Stitch(const Schema& other) const;

    const SchemaDefinition& Definition() const;

private:
    std::shared_ptr<SchemaDefinition> _schemaDefinition;
    Resolver _resolvers;
    Directives _directives;
    Scalars _scalars;

    Schema(const std::shared_ptr<SchemaDefinition>& schemaDefinition,
           const Resolver& resolvers,
           const Directives& directives,
           const Scalars& scalars);

    Task<GraphQLResponse> ResolveInternal(const std::string& query,
                                        const nlohmann::json& variables,
                                        const std::string& operationName,
                                        std::any context) const;

    SubscriptionHandle SubscribeInternal(const std::string& query,
                                         const nlohmann::json& variables,
                                         const std::string& operationName,
                                         std::any context) const;

    internal::ResolveQueryArgs BuildResolveQueryArgs(const std::string& query,
                                                     const nlohmann::json& variables,
                                                     const std::string& operationName,
                                                     std::any context) const;

    void InjectIntrospectionResolvers();
    void AddToResolver(const std::string& resolverName, const Resolver& resolver);
};

}
