#include "federation.h"

#include <format>
#include <gqlxy/internal/introspection/types/directive_definition.h>
#include <gqlxy/internal/introspection/types/field_definition.h>
#include <gqlxy/internal/introspection/types/input_value_definition.h>
#include <gqlxy/internal/introspection/types/type_definition.h>
#include <gqlxy/internal/merge_resolvers.h>
#include <gqlxy/parser/introspection/types/type_ref.h>
#include <gqlxy/resolver_args.h>
#include <gqlxy/utils/expect.h>
#include <gqlxy/utils/optional.h>
#include <gqlxy/utils/ranges.h>

using namespace std;
using namespace gqlxy::parser;
using namespace gqlxy::utils;

namespace gqlxy::internal {

static vector<string> Entities(const SchemaDefinition& schema) {
    return to_vector(schema.types | views::filter([](const auto& type) {
        return ranges::any_of(type.second.directives, [](const auto& directive) { return directive.name == "key"; });
    }) | views::keys);
}

static vector FederationScalarsAndTypes = {
    TypeDefinition {
        .kind = TypeKind::SCALAR,
        .name = "_Any"
    },
    TypeDefinition {
        .kind = TypeKind::SCALAR,
        .name = "_FieldSet"
    },
    TypeDefinition {
        .kind = TypeKind::OBJECT,
        .name = "_Service",
        .fields = {
            FieldDefinition {
                .name = "sdl",
                .type = TypeRef::Named("String")
            }
        }
    }
};

static void AddFederationScalarsAndTypes(SchemaDefinition& schema) {
    for (const auto& type : FederationScalarsAndTypes)
        schema.types.emplace(type.name, type);
}

static void AddEntityUnion(SchemaDefinition& schema, const vector<string>& entities) {
    if (entities.empty()) return;
    schema.types["_Entity"] = TypeDefinition {
        .kind = TypeKind::UNION,
        .name = "_Entity",
        .unionTypes = entities
    };
}

static vector FederationDirectives = {
    DirectiveDefinition {
        .name = "key",
        .locations = {DirectiveLocation::OBJECT, DirectiveLocation::INTERFACE},
        .args = {
            InputValueDefinition {
                .name = "fields",
                .type = TypeRef::NonNull(TypeRef::Named("_FieldSet"))
            },
            InputValueDefinition {
                .name = "resolvable",
                .type = TypeRef::Named("Boolean")
            }
        },
        .isRepeatable = true
    },
    DirectiveDefinition {
        .name = "external",
        .locations = {DirectiveLocation::OBJECT, DirectiveLocation::FIELD},
    },
    DirectiveDefinition {
        .name = "requires",
        .locations = {DirectiveLocation::FIELD},
        .args = {
            InputValueDefinition {
                .name = "fields",
                .type = TypeRef::NonNull(TypeRef::Named("_FieldSet"))
            }
        }
    },
    DirectiveDefinition {
        .name = "provides",
        .locations = {DirectiveLocation::FIELD},
        .args = {
            InputValueDefinition {
                .name = "fields",
                .type = TypeRef::NonNull(TypeRef::Named("_FieldSet"))
            }
        }
    },
    DirectiveDefinition {
        .name = "extends",
        .locations = {DirectiveLocation::OBJECT, DirectiveLocation::INTERFACE},
    },
    DirectiveDefinition {
        .name = "shareable",
        .locations = {DirectiveLocation::OBJECT, DirectiveLocation::FIELD},
        .isRepeatable = true
    },
    DirectiveDefinition {
        .name = "inaccessible",
        .locations = {DirectiveLocation::FIELD, DirectiveLocation::OBJECT, DirectiveLocation::INTERFACE}
    },
    DirectiveDefinition {
        .name = "override",
        .locations = {DirectiveLocation::FIELD},
        .args = {
            InputValueDefinition {
                .name = "from",
                .type = TypeRef::NonNull(TypeRef::Named("String"))
            }
        }
    },
    DirectiveDefinition {
        .name = "link",
        .locations = {DirectiveLocation::SCHEMA},
        .args = {
            InputValueDefinition {
                .name = "url",
                .type = TypeRef::NonNull(TypeRef::Named("String"))
            }
        },
        .isRepeatable = true
    }
};

static void AddFederationDirectives(SchemaDefinition& schema) {
    ranges::copy(FederationDirectives, back_inserter(schema.directives));
}

static void AddFederationQueryFields(SchemaDefinition& schema, bool hasEntities) {
    auto& query = schema.types[schema.queryTypeName.value_or("Query")];

    query.fields.push_back({
        .name = "_service",
        .type = TypeRef::NonNull(TypeRef::Named("_Service"))
    });

    if (!hasEntities) return;
    query.fields.push_back({
        .name = "_entities",
        .type = TypeRef::NonNullList(TypeRef::Named("_Entity")),
        .args = {
            InputValueDefinition {
                .name = "representations",
                .type = TypeRef::NonNullListNonNull(TypeRef::Named("_Any"))
            }
        }
    });
}

static void ValidateEntityResolvers(const vector<string>& entities, const Resolver& resolvers) {
    for (const auto& typeName : entities) {
        auto it = resolvers.find(typeName);
        auto resolver = it != resolvers.end() ? it->second.AsIf<Resolver>() : nullopt;
        expect(resolver && resolver->contains("__resolveReference"),
               format(R"(Federation: no __resolveReference resolver found for @key type "{}")", typeName));
    }
}

static unordered_map<string, FunctionResolver> ReferenceResolvers(const vector<string>& entities,
                                                                  const Resolver& resolvers) {
    return to_unordered_map(entities
        | views::transform([&](const string& keyType) {
            return make_pair(keyType, resolvers.at(keyType).As<Resolver>().at("__resolveReference").AsIf<FunctionResolver>());
        })
        | views::filter([](const auto& resolver) { return resolver.second.has_value(); })
        | views::transform([](const auto& resolver) { return make_pair(resolver.first, resolver.second.value()); }));
}

void InjectFederation(const shared_ptr<SchemaDefinition>& schema,
                      Resolver& resolvers,
                      const FederationOptions& options) {
    auto entities = Entities(*schema);
    ValidateEntityResolvers(entities, resolvers);

    AddFederationScalarsAndTypes(*schema);
    AddEntityUnion(*schema, entities);
    AddFederationDirectives(*schema);
    AddFederationQueryFields(*schema, !entities.empty());

    auto queryName = schema->queryTypeName.value_or("Query");
    if (!resolvers.contains(queryName)) resolvers[queryName] = Resolver {};
    auto& queryResolver = resolvers[queryName].As<Resolver>();
    queryResolver["_service"] = Resolver {
        {"sdl", options.typeDefs}
    };

    if (entities.empty()) return;

    MergeResolvers(resolvers, Resolver {
        {"_Entity", Resolver {
            {"__resolveType", TypeResolver([](const Resolver& current) -> optional<string> {
                auto it = current.find("__typename");
                return it != current.end() ? it->second.AsIf<string>() : nullopt;
            })}
        }}
    });
    MergeResolvers(queryResolver, Resolver {
        {"_entities", [refResolvers = ReferenceResolvers(entities, resolvers)](const ResolverArgs& r) {
            return to_vector(r.Args()["representations"]
                | views::transform([refResolvers](const nlohmann::json& rep) -> ValueResolver {
                    auto typeName = rep["__typename"].get<string>();
                    return or_else(
                        and_then(to_optional(refResolvers, refResolvers.find(typeName)),
                                 [&](const pair<string, FunctionResolver>& it) {
                                     auto entity = it.second(ResolverArgs {
                                         ResolverArgsParams {.args = rep}
                                     });
                                     if (entity.Is<Resolver>())
                                         entity.As<Resolver>().insert_or_assign("__typename", typeName);
                                     return make_optional(entity);
                                 }),
                        []() { return monostate {}; }
                    );
                })
            );
        }}
    });
}

}
