#include "introspection.h"
#include <ariane/internal/introspection/types/SchemaDefinition.h>
#include <ariane/internal/utils/optional.h>
#include <ariane/internal/utils/ranges.h>
#include <ranges>
#include <unordered_set>

using namespace std;
using namespace ariane::graphql::internal;
using namespace ariane::graphql;

namespace ariane::graphql::internal {

static const auto __Type = TypeRef::Named("__Type");
static const auto __NonNull__Type = TypeRef::NonNull(__Type);
static const auto __List__NonNull__Type = TypeRef::List(__NonNull__Type);
static const auto __NonNull__List__NonNull__Type = TypeRef::NonNull(__List__NonNull__Type);
static const auto _String = TypeRef::Named("String");
static const auto _Boolean = TypeRef::Named("Boolean");
static const auto __InputValue = TypeRef::Named("__InputValue");

static const vector builtInScalars = {
    TypeDefinition {
        .kind = TypeKind::SCALAR,
        .name = "String",
        .description = "The `String` scalar type represents textual data, represented as UTF-8 character sequences. The String type is most often used by GraphQL to represent free-form human-readable text."
    },
    TypeDefinition{
        .kind = TypeKind::SCALAR,
        .name = "Int",
        .description = "The `Int` scalar type represents non-fractional signed whole numeric values. Int can represent values between -(2^31) and 2^31 - 1."
    },
    TypeDefinition{
        .kind = TypeKind::SCALAR,
        .name = "Boolean",
        .description = "The `Boolean` scalar type represents `true` or `false`."
    },
    TypeDefinition{
        .kind = TypeKind::SCALAR,
        .name = "Float",
        .description = "The `Float` scalar type represents signed double-precision fractional values as specified by [IEEE 754](https://en.wikipedia.org/wiki/IEEE_floating_point)."
    },
    TypeDefinition{
        .kind = TypeKind::SCALAR,
        .name = "ID",
        .description = "The `ID` scalar type represents a unique identifier, often used to refetch an object or as key for a cache. The ID type appears in a JSON response as a String; however, it is not intended to be human-readable. When expected as an input type, any string (such as `\"4\"`) or integer (such as `4`) input value will be accepted as an ID."
    }
};

static const vector builtInDirectives = {
    DirectiveDefinition{
        .name = "include",
        .description = "Directs the executor to include this field or fragment only when the `if` argument is true.",
        .locations = {
            DirectiveLocation::FIELD,
            DirectiveLocation::FRAGMENT_SPREAD,
            DirectiveLocation::INLINE_FRAGMENT
        },
        .args = {
            InputValueDefinition {
                .name = "if",
                .description = "Included when true.",
                .type = TypeRef::NonNull(_Boolean)
            }
        }
    },
    DirectiveDefinition{
        .name = "skip",
        .description = "Directs the executor to skip this field or fragment when the `if` argument is true.",
        .locations = {
            DirectiveLocation::FIELD,
            DirectiveLocation::FRAGMENT_SPREAD,
            DirectiveLocation::INLINE_FRAGMENT
        },
        .args = {
            InputValueDefinition {
                .name = "if",
                .description = "Skipped when true.",
                .type = TypeRef::NonNull(_Boolean)
            }
        }
    },
    DirectiveDefinition{
        .name = "deprecated",
        .description = "Marks an element of a GraphQL schema as no longer supported.",
        .locations = {
            DirectiveLocation::FIELD_DEFINITION,
            DirectiveLocation::ARGUMENT_DEFINITION,
            DirectiveLocation::INPUT_FIELD_DEFINITION,
            DirectiveLocation::ENUM_VALUE
        },
        .args = {
            InputValueDefinition{
                .name = "reason",
                .description = "Explains why this element was deprecated, usually also including a suggestion for how to access supported similar data. Formatted using the Markdown syntax, as specified by [CommonMark](https://commonmark.org/).",
                .type = _String,
                .defaultValue = "\"No longer supported\""
            }
        }
    },
    DirectiveDefinition{
        .name = "specifiedBy",
        .description = "Exposes a URL that specifies the behavior of this scalar.",
        .locations = {DirectiveLocation::SCALAR},
        .args = {
            InputValueDefinition{
                .name = "url",
                .description = "The URL that specifies the behavior of this scalar.",
                .type = TypeRef::NonNull(_String)
            }
        }
    },
    DirectiveDefinition{
        .name = "oneOf",
        .description = "Indicates exactly one field must be supplied and this field must not be `null`.",
        .locations = {DirectiveLocation::INPUT_OBJECT},
        .args = {}
    }
};

static const auto includeDeprecatedArgs = InputValueDefinition{
    .name = "includeDeprecated",
    .description = nullopt,
    .type = _Boolean,
    .defaultValue = "false"
};

static const vector introspectionTypeDefinitions = {
    TypeDefinition{
        .kind = TypeKind::OBJECT,
        .name = "__Schema",
        .description = "A GraphQL Schema defines the capabilities of a GraphQL server. It exposes all available types and directives on the server, as well as the entry points for query, mutation, and subscription operations.",
        .fields = {
            FieldDefinition {
                .name = "description",
                .type = _String
            },
            FieldDefinition {
                .name = "types",
                .description = "A list of all types supported by this server.",
                .type = __NonNull__List__NonNull__Type
            },
            FieldDefinition {
                .name = "queryType",
                .description = "The type that query operations will be rooted at.",
                .type = __NonNull__Type
            },
            FieldDefinition {
                .name = "mutationType",
                .description = "If this server supports mutation, the type that mutation operations will be rooted at.",
                .type = __Type
            },
            FieldDefinition {
                .name = "subscriptionType",
                .description = "If this server support subscription, the type that subscription operations will be rooted at.",
                .type = __Type
            },
            FieldDefinition {
                .name = "directives",
                .description = "A list of all directives supported by this server.",
                .type = TypeRef::NonNullListNonNull(TypeRef::Named("__Directive"))
            }
        },
        .interfaces = {}
    },
    TypeDefinition{
        .kind = TypeKind::OBJECT,
        .name = "__Type",
        .description = "The fundamental unit of any GraphQL Schema is the type. There are many kinds of types in GraphQL as represented by the `__TypeKind` enum.\n\nDepending on the kind of a type, certain fields describe information about that type. Scalar types provide no information beyond a name, description and optional `specifiedByURL`, while Enum types provide their values. Object and Interface types provide the fields they describe. Abstract types, Union and Interface, provide the Object types possible at runtime. List and NonNull types compose other types.",
        .fields = {
            FieldDefinition {
                .name = "kind",
                .type = TypeRef::NonNull(TypeRef::Named("__TypeKind"))
            },
            FieldDefinition {
                .name = "name",
                .type = _String
            },
            FieldDefinition {
                .name = "description",
                .type = _String
            },
            FieldDefinition {
                .name = "specifiedByURL",
                .type = _String
            },
            FieldDefinition {
                .name = "fields",
                .type = TypeRef::ListNonNull(TypeRef::Named("__Field")),
                .args = {includeDeprecatedArgs}
            },
            FieldDefinition {
                .name = "interfaces",
                .type = __List__NonNull__Type
            },
            FieldDefinition {
                .name = "possibleTypes",
                .type = __List__NonNull__Type
            },
            FieldDefinition {
                .name = "enumValues",
                .type = TypeRef::ListNonNull(TypeRef::Named("__EnumValue")),
                .args = {includeDeprecatedArgs}
            },
            FieldDefinition {
                .name = "inputFields",
                .type = TypeRef::ListNonNull(__InputValue),
                .args = {includeDeprecatedArgs}
            },
            FieldDefinition {
                .name = "ofType",
                .type = __Type
            },
            FieldDefinition {
                .name = "isOneOf",
                .type = _Boolean
            }
        },
        .interfaces = {}
    },
    TypeDefinition {
        .kind = TypeKind::ENUM,
        .name = "__TypeKind",
        .description = "An enum describing what kind of type a given `__Type` is.",
        .enumValues = {
            EnumValueDefinition {
                .name = "SCALAR",
                .description = "Indicates this type is a scalar."
            },
            EnumValueDefinition {
                .name = "OBJECT",
                .description = "Indicates this type is an object. `fields` and `interfaces` are valid fields."
            },
            EnumValueDefinition {
                .name = "INTERFACE",
                .description = "Indicates this type is an interface. `fields`, `interfaces`, and `possibleTypes` are valid fields."
            },
            EnumValueDefinition {
                .name = "UNION",
                .description = "Indicates this type is a union. `possibleTypes` is a valid field."
            },
            EnumValueDefinition {
                .name = "ENUM",
                .description = "Indicates this type is an enum. `enumValues` is a valid field."
            },
            EnumValueDefinition {
                .name = "INPUT_OBJECT",
                .description = "Indicates this type is an input object. `inputFields` is a valid field."
            },
            EnumValueDefinition {
                .name = "LIST",
                .description = "Indicates this type is a list. `ofType` is a valid field."
            },
            EnumValueDefinition {
                .name = "NON_NULL",
                .description = "Indicates this type is a non-null. `ofType` is a valid field."
            }
        }
    },
    TypeDefinition {
        .kind = TypeKind::OBJECT,
        .name = "__Field",
        .description = "Object and Interface types are described by a list of Fields, each of which has a name, potentially a list of arguments, and a return type.",
        .fields = {
            FieldDefinition {
                .name = "name",
                .type = TypeRef::NonNull(_String)
            },
            FieldDefinition {
                .name = "description",
                .type = _String
            },
            FieldDefinition {
                .name = "args",
                .type = TypeRef::NonNullListNonNull(__InputValue),
                .args = {includeDeprecatedArgs}
            },
            FieldDefinition {
                .name = "type",
                .type = __NonNull__Type
            },
            FieldDefinition {
                .name = "isDeprecated",
                .type = TypeRef::NonNull(_Boolean)
            },
            FieldDefinition {
                .name = "deprecationReason",
                .type = _String
            }
        },
        .interfaces = {}
    },
    TypeDefinition{
        .kind = TypeKind::OBJECT,
        .name = "__InputValue",
        .description = "Arguments provided to Fields or Directives and the input fields of an InputObject are represented as Input Values which describe their type and optionally a default value.",
        .fields = {
            FieldDefinition {
                .name = "name",
                .type = TypeRef::NonNull(_String)
            },
            FieldDefinition {
                .name = "description",
                .type = _String
            },
            FieldDefinition {
                .name = "type",
                .type = __NonNull__Type
            },
            FieldDefinition {
                .name = "defaultValue",
                .description = "A GraphQL-formatted string representing the default value for this input value.",
                .type = _String
            },
            FieldDefinition {
                .name = "isDeprecated",
                .type = TypeRef::NonNull(_Boolean)
            },
            FieldDefinition {
                .name = "deprecationReason",
                .type = _String
            }
        },
        .interfaces = {}
    },
    TypeDefinition{
        .kind = TypeKind::OBJECT,
        .name = "__EnumValue",
        .description = "One possible value for a given Enum. Enum values are unique values, not a placeholder for a string or numeric value. However an Enum value is returned in a JSON response as a string.",
        .fields = {
            FieldDefinition {
                .name = "name",
                .type = TypeRef::NonNull(_String)
            },
            FieldDefinition {
                .name = "description",
                .type = _String
            },
            FieldDefinition {
                .name = "isDeprecated",
                .type = TypeRef::NonNull(_Boolean)
            },
            FieldDefinition {
                .name = "deprecationReason",
                .type = _String
            }
        },
        .interfaces = {}
    },
    TypeDefinition{
        .kind = TypeKind::OBJECT,
        .name = "__Directive",
        .description = "A Directive provides a way to describe alternate runtime execution and type validation behavior in a GraphQL document.\n\nIn some cases, you need to provide options to alter GraphQL's execution behavior in ways field arguments will not suffice, such as conditionally including or skipping a field. Directives provide this by describing additional information to the executor.",
        .fields = {
            FieldDefinition {
                .name = "name",
                .type = TypeRef::NonNull(_String)
            },
            FieldDefinition {
                .name = "description",
                .type = _String
            },
            FieldDefinition {
                .name = "isRepeatable",
                .type = TypeRef::NonNull(_Boolean)
            },
            FieldDefinition {
                .name = "locations",
                .type = TypeRef::NonNullListNonNull(TypeRef::Named("__DirectiveLocation"))
            },
            FieldDefinition {
                .name = "args",
                .type = TypeRef::NonNullListNonNull(__InputValue),
                .args = {includeDeprecatedArgs}
            }
        },
        .interfaces = {}
    },
    TypeDefinition{
        .kind = TypeKind::ENUM,
        .name = "__DirectiveLocation",
        .description = "A Directive can be adjacent to many parts of the GraphQL language, a __DirectiveLocation describes one such possible adjacencies.",
        .enumValues = {
            EnumValueDefinition {
                .name = "QUERY",
                .description = "Location adjacent to a query operation."
            },
            EnumValueDefinition {
                .name = "MUTATION",
                .description = "Location adjacent to a mutation operation."
            },
            EnumValueDefinition {
                .name = "SUBSCRIPTION",
                .description = "Location adjacent to a subscription operation."
            },
            EnumValueDefinition {
                .name = "FIELD",
                .description = "Location adjacent to a field."
            },
            EnumValueDefinition {
                .name = "FRAGMENT_DEFINITION",
                .description = "Location adjacent to a fragment definition."
            },
            EnumValueDefinition {
                .name = "FRAGMENT_SPREAD",
                .description = "Location adjacent to a fragment spread."
            },
            EnumValueDefinition {
                .name = "INLINE_FRAGMENT",
                .description = "Location adjacent to an inline fragment."
            },
            EnumValueDefinition {
                .name = "VARIABLE_DEFINITION",
                .description = "Location adjacent to a variable definition."
            },
            EnumValueDefinition {
                .name = "SCHEMA",
                .description = "Location adjacent to a schema definition."
            },
            EnumValueDefinition {
                .name = "SCALAR",
                .description = "Location adjacent to a scalar definition."
            },
            EnumValueDefinition {
                .name = "OBJECT",
                .description = "Location adjacent to an object type definition."
            },
            EnumValueDefinition {
                .name = "FIELD_DEFINITION",
                .description = "Location adjacent to a field definition."
            },
            EnumValueDefinition {
                .name = "ARGUMENT_DEFINITION",
                .description = "Location adjacent to an argument definition."
            },
            EnumValueDefinition {
                .name = "INTERFACE",
                .description = "Location adjacent to an interface definition."
            },
            EnumValueDefinition {
                .name = "UNION",
                .description = "Location adjacent to a union definition."
            },
            EnumValueDefinition {
                .name = "ENUM",
                .description = "Location adjacent to an enum definition."
            },
            EnumValueDefinition {
                .name = "ENUM_VALUE",
                .description = "Location adjacent to an enum value definition."
            },
            EnumValueDefinition {
                .name = "INPUT_OBJECT",
                .description = "Location adjacent to an input object type definition."
            },
            EnumValueDefinition {
                .name = "INPUT_FIELD_DEFINITION",
                .description = "Location adjacent to an input object field definition."
            }
        }
    }
};

static auto introspectionTypeMap = to_map(views::transform(introspectionTypeDefinitions, [](const auto& typeDef) {
    return make_pair(typeDef.name, typeDef);
}));

static auto introspectionTypeNames = views::transform(introspectionTypeDefinitions, [](const auto& typeDef) {
    return typeDef.name;
});

static auto builtinScalarsMap = to_map(builtInScalars | views::transform([](const auto& typeDef) {
    return make_pair(typeDef.name, typeDef);
}));

static string resolveKind(const string& typeName, const SchemaDefinition& schemaDefinition) {
    if (schemaDefinition.types.contains(typeName)) return schemaDefinition.types.at(typeName).kind._to_string();
    if (builtinScalarsMap.contains(typeName))
        return "SCALAR";

    auto it = ranges::find_if(introspectionTypeDefinitions, [&](const auto& t) { return t.name == typeName; });
    if (it != introspectionTypeDefinitions.end()) return it->kind._to_string();
    return "OBJECT";
}

Resolver CreateTypeRefResolver(const TypeRef& typeRef, const SchemaDefinition& schemaDefinition) {
    if (typeRef.kind._value == TypeRefKind::NON_NULL) {
        return Resolver {
            {"kind", "NON_NULL"},
            {"name", nullopt},
            {"ofType", [ofType = typeRef.ofType, &schemaDefinition](const auto&) {
                return ofType != nullptr ? make_optional(CreateTypeRefResolver(*ofType, schemaDefinition)) : nullopt;
            }}
        };
    }
    if (typeRef.kind._value == TypeRefKind::LIST) {
        return Resolver {
            {"kind", "LIST"},
            {"name", nullopt},
            {"ofType", [ofType = typeRef.ofType, &schemaDefinition](const auto&) {
                return ofType != nullptr ? make_optional(CreateTypeRefResolver(*ofType, schemaDefinition)) : nullopt;
            }}
        };
    }

    return Resolver {
        {"kind", resolveKind(typeRef.name, schemaDefinition)},
        {"name", typeRef.name},
        {"ofType", monostate{}}
    };
}

static bool IsDeprecated(const optional<DeprecationInfo>& deprecation) {
    return or_else(and_then(deprecation, [](const auto& dep) {
        return make_optional(dep.isDeprecated);
    }), []() { return false; }).value();
}

static optional<string> DeprecationReason(const optional<DeprecationInfo>& deprecation) {
    return and_then(deprecation, [](const auto& dep) {
        return dep.deprecationReason;
    });
}

Resolver CreateInputValueResolver(const InputValueDefinition& input, const SchemaDefinition& schemaDefinition) {
    return Resolver {
        {"name", input.name},
        {"description", input.description},
        {"type", [input, &schemaDefinition](const auto&) { return CreateTypeRefResolver(input.type, schemaDefinition); }},
        {"defaultValue", input.defaultValue}
    };
}

Resolver CreateEnumValueResolver(const EnumValueDefinition& enumValue) {
    return Resolver {
        {"name", enumValue.name},
        {"description", enumValue.description},
        {"isDeprecated", IsDeprecated(enumValue.deprecation)},
        {"deprecationReason", DeprecationReason(enumValue.deprecation)}
    };
}

Resolver CreateFieldResolver(const FieldDefinition& field, const SchemaDefinition& schemaDefinition) {
    return Resolver {
        {"name", field.name},
        {"description", field.description},
        {"type", CreateTypeRefResolver(field.type, schemaDefinition)},
        {"isDeprecated", IsDeprecated(field.deprecation)},
        {"deprecationReason", DeprecationReason(field.deprecation)},
        {"args", [args = field.args, &schemaDefinition](const auto&) {
            return to_vector(args | views::transform([&schemaDefinition](const auto& a) -> ValueResolver {
                return CreateInputValueResolver(a, schemaDefinition);
            }));
        }}
    };
}

Resolver CreateDirectiveResolver(const DirectiveDefinition& directive, const SchemaDefinition& schemaDefinition) {
    return Resolver {
        {"name", directive.name},
        {"description", directive.description},
        {"isRepeatable", directive.isRepeatable},
        {"locations", [locations = directive.locations](const auto&) {
            return to_vector(locations | views::transform([](const auto& location) -> ValueResolver {
                return location._to_string();
            }));
        }},
        {"args", [args = directive.args, &schemaDefinition](const auto&) {
            return to_vector(args | views::transform([&schemaDefinition](const auto& a) -> ValueResolver {
                return CreateInputValueResolver(a, schemaDefinition);
            }));
        }}
    };
}

Resolver CreateTypeResolver(const TypeDefinition& type, const SchemaDefinition& schemaDefinition) {
    return Resolver {
        {"kind", string(type.kind._to_string())},
        {"name", type.name},
        {"description", type.description},
        {"fields", [type, &schemaDefinition](const auto&) -> ValueResolver {
            switch (type.kind._value) {
                case TypeKind::OBJECT:
                case TypeKind::INTERFACE: {
                    return to_vector(type.fields | views::transform([&schemaDefinition](const auto& field) -> ValueResolver {
                        return CreateFieldResolver(field, schemaDefinition);
                    }));
                }
                default:
                    return monostate{};
            }
        }},
        {"interfaces", [type, &schemaDefinition](const auto&) -> ValueResolver {
            if (type.kind._value == TypeKind::OBJECT) {
                return to_vector(type.interfaces | views::transform([&schemaDefinition](const auto& interface) -> ValueResolver {
                    return CreateTypeRefResolver(TypeRef::Named(interface), schemaDefinition);
                }));
            }
            if (type.kind._value == TypeKind::INTERFACE) {
                return vector<ValueResolver>{};
            }
            return monostate{};
        }},
        {"possibleTypes", [type, &schemaDefinition](const auto&) -> ValueResolver {
            switch (type.kind._value) {
                case TypeKind::INTERFACE: {
                    return to_vector(type.possibleTypes | views::transform([&schemaDefinition](const auto& pt) -> ValueResolver {
                        return CreateTypeRefResolver(TypeRef::Named(pt), schemaDefinition);
                    }));
                }
                case TypeKind::UNION: {
                    return to_vector(type.unionTypes | views::transform([&schemaDefinition](const auto& ut) -> ValueResolver {
                        return CreateTypeRefResolver(TypeRef::Named(ut), schemaDefinition);
                    }));
                }
                default:
                    return monostate{};
            }
        }},
        {"enumValues", [type](const auto&) -> ValueResolver {
            if (type.kind._value != TypeKind::ENUM)
                return monostate{};

            return to_vector(type.enumValues | views::transform([](const auto& enumValue) -> ValueResolver {
                return CreateEnumValueResolver(enumValue);
            }));
        }},
        {"inputFields", [type, &schemaDefinition](const auto&) -> ValueResolver {
            if (type.kind._value != TypeKind::INPUT_OBJECT)
                return monostate{};

            return to_vector(type.inputFields | views::transform([&schemaDefinition](const auto& field) -> ValueResolver {
                return CreateInputValueResolver(field, schemaDefinition);
            }));
        }}
    };
}

//TODO Refactor this
static void collectTypeName(
    const string& typeName,
    unordered_set<string>& visited,
    vector<string>& result,
    const SchemaDefinition& schemaDefinition
) {
    if (visited.contains(typeName)) return;
    visited.insert(typeName);
    result.push_back(typeName);

    if (!schemaDefinition.types.contains(typeName)) return; // built-in or unknown: no further DFS
    const auto& type = schemaDefinition.types.at(typeName);

    for (const auto& iface : type.interfaces)
        collectTypeName(iface, visited, result, schemaDefinition);
    for (const auto& ut : type.unionTypes)
        collectTypeName(ut, visited, result, schemaDefinition);
    for (const auto& f : type.inputFields)
        collectTypeName(f.type.typeName(), visited, result, schemaDefinition);
    for (const auto& field : type.fields) {
        collectTypeName(field.type.typeName(), visited, result, schemaDefinition);
        for (const auto& arg : field.args)
            collectTypeName(arg.type.typeName(), visited, result, schemaDefinition);
    }
}

//TODO Refactor this
static vector<string> buildTypeOrder(const SchemaDefinition& schemaDefinition) {
    auto sortedNames = to_set(schemaDefinition.types | views::keys);
    unordered_set<string> visited(sortedNames.begin(), sortedNames.end());
    vector<string> result;

    for (const auto& name : sortedNames) {
        visited.erase(name); // unmark so DFS will process it
        collectTypeName(name, visited, result, schemaDefinition);
    }

    for (const auto& name : introspectionTypeNames) {
        if (!visited.contains(name)) {
            visited.insert(name);
            result.push_back(name);
        }
    }

    return result;
}

static optional<TypeDefinition> GetTypeDefinition(const SchemaDefinition& schemaDefinition, const string& name) {
    if (schemaDefinition.types.contains(name)) return schemaDefinition.types.at(name);
    if (builtinScalarsMap.contains(name)) return builtinScalarsMap.at(name);
    if (introspectionTypeMap.contains(name)) return introspectionTypeMap.at(name);
    return nullopt;
}

Resolver CreateSchemaResolver(const SchemaDefinition& schemaDefinition) {
    auto orderedTypes = to_vector(buildTypeOrder(schemaDefinition)
        | views::filter([&schemaDefinition](const auto& type) { return GetTypeDefinition(schemaDefinition, type).has_value(); })
        | views::transform([&schemaDefinition](const auto& type) { return GetTypeDefinition(schemaDefinition, type); }));

    vector<DirectiveDefinition> allDirectives = schemaDefinition.directives;
    ranges::copy(builtInDirectives, back_inserter(allDirectives));

    return Resolver {
        {"queryType", and_then(schemaDefinition.queryTypeName, [](const auto& name) {
            return Resolver{
                {"name", name},
                {"kind", "OBJECT"}
            };
        })},
        {"mutationType", and_then(schemaDefinition.mutationTypeName, [](const auto& name) {
            return Resolver{
                {"name", name},
                {"kind", "OBJECT"}
            };
        })},
        {"subscriptionType", and_then(schemaDefinition.subscriptionTypeName, [](const auto& name) {
            return Resolver{
                {"name", name},
                {"kind", "OBJECT"}
            };
        })},
        {"directives", [allDirectives, &schemaDefinition](const auto&) {
            return to_vector(allDirectives | views::transform([&schemaDefinition](const auto& d) -> ValueResolver {
                return CreateDirectiveResolver(d, schemaDefinition);
            }));
        }},
        {"types", [orderedTypes, &schemaDefinition](const auto&) {
            return to_vector(orderedTypes | views::transform([&schemaDefinition](const auto& type) -> ValueResolver {
                return CreateTypeResolver(type.value(), schemaDefinition);
            }));
        }}
    };
}

}
