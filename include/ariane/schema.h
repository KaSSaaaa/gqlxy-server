#pragma once

#include <any>
#include <ariane/resolvers.h>
#include <ariane/results.h>
#include <ariane/subscription.h>
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

template <typename TContext = std::monostate>
struct SchemaResolveArgs {
    std::string query;
    nlohmann::json variables = nlohmann::json::object();
    std::string operationName;
    TContext context = {};
};

class Schema {
public:
    explicit Schema(const SchemaOptions& options);

    template <typename TContext = std::monostate>
    Task<ResolveResult> Resolve(const SchemaResolveArgs<TContext>& args) const {
        return ResolveInternal(args.query, args.variables, args.operationName, args.context);
    }

    template <typename TContext = std::monostate>
    SubscriptionHandle Subscribe(const SchemaResolveArgs<TContext>& args) const {
        return SubscribeInternal(args.query, args.variables, args.operationName, args.context);
    }

    Schema Stitch(const Schema& other) const;

private:
    std::shared_ptr<internal::SchemaDefinition> _schemaDefinition;
    Resolver _resolvers;
    Directives _directives;
    Scalars _scalars;

    Schema(const std::shared_ptr<internal::SchemaDefinition>& schemaDefinition,
           const Resolver& resolvers,
           const Directives& directives,
           const Scalars& scalars);

    Task<ResolveResult> ResolveInternal(const std::string& query,
                                        const nlohmann::json& variables,
                                        const std::string& operationName,
                                        std::any context) const;

    SubscriptionHandle SubscribeInternal(const std::string& query,
                                         const nlohmann::json& variables,
                                         const std::string& operationName,
                                         std::any context) const;

    void InjectIntrospectionResolvers();
    void AddToResolver(const std::string& resolverName, const Resolver& resolver);
};

}