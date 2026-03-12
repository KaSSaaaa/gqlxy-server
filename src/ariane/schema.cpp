#include <ariane/schema.h>

#include <ariane/internal/MergeResolvers.h>
#include <ariane/internal/engine/resolve.h>
#include <ariane/internal/engine/subscribe.h>
#include <ariane/internal/introspection/introspection.h>
#include <ariane/internal/introspection/types/SchemaDefinition.h>
#include <ariane/internal/peg/parser/schema_parser.h>

using namespace std;
using namespace ariane::graphql;
using namespace ariane::graphql::internal;
using namespace graphql;

static const Directives builtinDirectives = {
    {"skip", [](const ResolverArgs& args, const ValueResolver& v) -> optional<ValueResolver> {
        return args.Args().value("if", false) ? nullopt : make_optional(v);
    }},
    {"include", [](const ResolverArgs& args, const ValueResolver& v) -> optional<ValueResolver> {
        return args.Args().value("if", true) ? make_optional(v) : nullopt;
    }},
};

Schema::Schema(const SchemaOptions& options) {
    _resolvers = options.resolvers;
    _schemaDefinition = ParseSchemaDefinition(options.typeDefs);

    for (const auto& [name, type] : _schemaDefinition->types)
        if (type.kind._value == TypeKind::OBJECT)
            AddToResolver(name, Resolver {
                {"__typename", name},
                {"__resolveType", name}
            });

    if (options.allowIntrospection)
        InjectIntrospectionResolvers();

    _directives = builtinDirectives;
    for (const auto& [name, fn] : options.directives)
        _directives[name] = fn;
    _scalars = options.scalars;
}

void Schema::InjectIntrospectionResolvers() {
    AddToResolver("Query", Resolver {
        {"__schema", [schemaDefinition = _schemaDefinition](const ResolverArgs&) {
            return CreateSchemaResolver(*schemaDefinition);
        }},
        {"__type", [schemaDefinition = _schemaDefinition](const ResolverArgs& args) -> ValueResolver {
            if (auto it = schemaDefinition->types.find(args.Args()["name"]); it != schemaDefinition->types.end()) {
                return CreateTypeResolver(it->second, *schemaDefinition);
            }
            return nullopt;
        }}
    });
}

void Schema::AddToResolver(const string& resolverName, const Resolver& resolver) {
    if (!_resolvers.contains(resolverName)) {
        _resolvers[resolverName] = Resolver{};
    }
    MergeResolvers(_resolvers.at(resolverName).As<Resolver>(), resolver);
}

Task<ResolveResult> Schema::ResolveInternal(const string& query,
                                            const nlohmann::json& variables,
                                            const string& operationName,
                                            any context) const {
    return ResolveOperations(ResolveQueryArgs {
        .query = query,
        .variables = variables,
        .schemaDefinition = *_schemaDefinition,
        .resolvers = _resolvers,
        .directives = _directives,
        .scalars = _scalars,
        .operationName = operationName,
        .context = std::move(context),
    });
}

SubscriptionHandle Schema::SubscribeInternal(const string& query,
                                                  const nlohmann::json& variables,
                                                  const string& operationName,
                                                  any context) const {
    return internal::Subscribe(ResolveQueryArgs {
        .query = query,
        .variables = variables,
        .schemaDefinition = *_schemaDefinition,
        .resolvers = _resolvers,
        .directives = _directives,
        .scalars = _scalars,
        .operationName = operationName,
        .context = std::move(context),
    });
}
