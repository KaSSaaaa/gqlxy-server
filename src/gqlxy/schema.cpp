#include <gqlxy/schema.h>

#include <gqlxy/ResolverArgs.h>
#include <gqlxy/internal/MergeResolvers.h>
#include <gqlxy/internal/ast/BuildInScalars.h>
#include <gqlxy/internal/engine/federation.h>
#include <gqlxy/internal/engine/resolve.h>
#include <gqlxy/internal/engine/subscribe.h>
#include <gqlxy/internal/introspection/introspection.h>
#include <gqlxy/internal/introspection/types/SchemaDefinition.h>
#include <gqlxy/internal/peg/parser/schema_parser.h>
#include <gqlxy/internal/utils/expect.h>
#include <format>

using namespace std;
using namespace gqlxy;
using namespace gqlxy::internal;
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

    if (options.federation)
        InjectFederation(_schemaDefinition, _resolvers, {
            .typeDefs = options.typeDefs
        });

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

Schema::Schema(const shared_ptr<SchemaDefinition>& schemaDefinition,
               const Resolver& resolvers,
               const Directives& directives,
               const Scalars& scalars)
    : _schemaDefinition(schemaDefinition),
      _resolvers(resolvers),
      _directives(directives),
      _scalars(scalars) {
    if (_resolvers.contains("Query")) {
        auto& query = _resolvers["Query"].As<Resolver>();
        query.erase("__schema");
        query.erase("__type");
    }
    InjectIntrospectionResolvers();
}

static bool IsRootType(const string& name, const SchemaDefinition& schema) {
    return name == schema.queryTypeName.value_or("Query") ||
           name == schema.mutationTypeName.value_or("Mutation") ||
           name == schema.subscriptionTypeName.value_or("Subscription");
}

static bool IsRootType(const string& name, const SchemaDefinition& other, const SchemaDefinition& merged) {
    return IsRootType(name, merged) || IsRootType(name, other);
}

static void MergeTypes(SchemaDefinition& merged, const SchemaDefinition& other) {
    for (const auto& [name, type] : other.types | views::filter([](const auto& kvp) {
        return !kvp.first.starts_with("__") && !BuiltinScalarsMap.contains(kvp.first);
    })) {
        if (IsRootType(name, other, merged)) {
            auto& existing = merged.types[name];
            for (const auto& field : type.fields) {
                auto conflict = ranges::find_if(existing.fields, [&](const auto& f) { return f.name == field.name; });
                expect(conflict == existing.fields.end(), format("Conflicting field '{}' in stitched type '{}'", field.name, name));
                existing.fields.push_back(field);
            }
        } else {
            expect(!merged.types.contains(name), format("Duplicate type '{}' in stitched schema", name));
            merged.types[name] = type;
        }
    }

    for (auto& type : merged.types | views::values)
        type.possibleTypes.clear();

    for (const auto& [name, interface] : merged.InterfacesPerType())
        merged.types[interface].possibleTypes.push_back(name);
}

static void MergeSchemaDirectives(SchemaDefinition& merged, const SchemaDefinition& other) {
    ranges::copy(other.directives | views::filter([&](const auto& dir) {
        return ranges::find_if(merged.directives, [&](const auto& d) { return d.name == dir.name; }) == merged.directives.end();
    }), back_inserter(merged.directives));
}

static void MergeSchemaNames(SchemaDefinition& merged, const SchemaDefinition& other) {
    if (!merged.queryTypeName) merged.queryTypeName = other.queryTypeName;
    if (!merged.mutationTypeName) merged.mutationTypeName = other.mutationTypeName;
    if (!merged.subscriptionTypeName) merged.subscriptionTypeName = other.subscriptionTypeName;
}

Schema Schema::Stitch(const Schema& other) const {
    auto merged = make_shared<SchemaDefinition>(*_schemaDefinition);
    MergeTypes(*merged, *other._schemaDefinition);
    MergeSchemaDirectives(*merged, *other._schemaDefinition);
    MergeSchemaNames(*merged, *other._schemaDefinition);
    return {
        merged,
        MergeResolvers(_resolvers, other._resolvers),
        concat(_directives, other._directives),
        concat(_scalars, other._scalars)
    };
}

void Schema::InjectIntrospectionResolvers() {
    AddToResolver("Query", Resolver {
        {"__schema", [schemaDefinition = _schemaDefinition](const ResolverArgs&) {
            return CreateSchemaResolver(*schemaDefinition);
        }},
        {"__type", [schemaDefinition = _schemaDefinition](const ResolverArgs& args) -> ValueResolver {
            auto typeDef = GetTypeDefinition(*schemaDefinition, args.Args()["name"].get<std::string>());
            if (!typeDef) return nullopt;
            return CreateTypeResolver(*typeDef, *schemaDefinition);
        }}
    });
}

void Schema::AddToResolver(const string& resolverName, const Resolver& resolver) {
    if (!_resolvers.contains(resolverName)) _resolvers[resolverName] = Resolver {};
    MergeResolvers(_resolvers.at(resolverName).As<Resolver>(), resolver);
}

Task<ResolveResult> Schema::ResolveInternal(const string& query,
                                            const nlohmann::json& variables,
                                            const string& operationName,
                                            any context) const {
    return ResolveOperations(BuildResolveQueryArgs(query, variables, operationName, std::move(context)));
}

SubscriptionHandle Schema::SubscribeInternal(const string& query,
                                             const nlohmann::json& variables,
                                             const string& operationName,
                                             any context) const {
    return internal::Subscribe(BuildResolveQueryArgs(query, variables, operationName, std::move(context)));
}

ResolveQueryArgs Schema::BuildResolveQueryArgs(const string& query,
                                                const nlohmann::json& variables,
                                                const string& operationName,
                                                any context) const {
    return ResolveQueryArgs {
        .query = query,
        .variables = variables,
        .schemaDefinition = *_schemaDefinition,
        .resolvers = _resolvers,
        .directives = _directives,
        .scalars = _scalars,
        .operationName = operationName,
        .context = std::move(context),
    };
}
