#include <ariane/schema.h>

#include <ariane/internal/MergeResolvers.h>
#include <ariane/internal/engine/resolve.h>
#include <ariane/internal/introspection/introspection.h>
#include <ariane/internal/introspection/types/SchemaDefinition.h>
#include <ariane/internal/peg/parser/schema_parser.h>

using namespace std;
using namespace ariane::graphql;
using namespace ariane::graphql::internal;
using namespace graphql;

static const Directives builtinDirectives = {
    {"skip", [](const ResolverArgs& args, const ValueResolver& v) -> optional<ValueResolver> {
        return args.args.value("if", false) ? nullopt : make_optional(v);
    }},
    {"include", [](const ResolverArgs& args, const ValueResolver& v) -> optional<ValueResolver> {
        return args.args.value("if", true) ? make_optional(v) : nullopt;
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
}

void Schema::InjectIntrospectionResolvers() {
    AddToResolver("Query", Resolver {
        {"__schema", [schemaDefinition = _schemaDefinition](const ResolverArgs&) {
            return CreateSchemaResolver(*schemaDefinition);
        }},
        {"__type", [schemaDefinition = _schemaDefinition](const ResolverArgs& args) -> ValueResolver {
            if (auto it = schemaDefinition->types.find(args.args["name"]); it != schemaDefinition->types.end()) {
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
    MergeResolvers(get<Resolver>(_resolvers.at(resolverName)), resolver);
}

Task<ResolveResult> Schema::Resolve(const SchemaResolveArgs& args) const {
    return ResolveOperations(ResolveQueryArgs {
        .query = args.query,
        .variables = args.variables,
        .schemaDefinition = *_schemaDefinition,
        .resolvers = _resolvers,
        .directives = _directives,
    });
}
