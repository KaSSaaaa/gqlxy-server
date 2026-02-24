#include "introspection.h"
#include <ariane/internal/introspection/types/Document.h>
#include <ariane/internal/utils/optional.h>
#include <ranges>
#include <unordered_set>

#include "ariane/internal/utils/ranges.h"

using namespace std;
using namespace ariane::graphql::internal;
using namespace ariane::graphql;

namespace ariane::graphql::internal {

static TypeRef __Type = TypeRef::Named("__Type");
static TypeRef __NonNull__Type = TypeRef::NonNull(__Type);
static TypeRef __List__NonNull__Type = TypeRef::List(__NonNull__Type);
static TypeRef __NonNull__List__NonNull__Type = TypeRef::NonNull(__List__NonNull__Type);
static TypeRef _String = TypeRef::Named("String");
static TypeRef _Boolean = TypeRef::Named("Boolean");
static TypeRef __InputValue = TypeRef::Named("__InputValue");

static string getNamedTypeName(const TypeRef& typeRef) {
    if (typeRef.kind._value == TypeRefKind::NamedType) return typeRef.name;
    if (typeRef.ofType) return getNamedTypeName(*typeRef.ofType);
    return "";
}

static string resolveKind(const string& typeName, const Document& doc) {
    if (doc.types.contains(typeName)) return doc.types.at(typeName).kind._to_string();
    static const unordered_set<string> scalars = {"String", "Int", "Boolean", "Float", "ID"};
    if (scalars.contains(typeName)) return "SCALAR";
    static const unordered_map<string, string> introspection = {
        {"__Schema", "OBJECT"},
        {"__Type", "OBJECT"},
        {"__Field", "OBJECT"},
        {"__InputValue", "OBJECT"},
        {"__EnumValue", "OBJECT"},
        {"__Directive", "OBJECT"},
        {"__TypeKind", "ENUM"},
        {"__DirectiveLocation", "ENUM"}
    };
    if (introspection.count(typeName)) return introspection.at(typeName);
    return "OBJECT";
}

Resolver CreateTypeRefResolver(const TypeRef& typeRef, const Document& doc) {
    if (typeRef.kind._value == TypeRefKind::NON_NULL) {
        return Resolver {
            {"kind", "NON_NULL"},
            {"name", nullopt},
            {"ofType", [ofType = typeRef.ofType, &doc]() {
                return ofType != nullptr ? make_optional(CreateTypeRefResolver(*ofType, doc)) : nullopt;
            }}
        };
    }
    if (typeRef.kind._value == TypeRefKind::LIST) {
        return Resolver {
            {"kind", "LIST"},
            {"name", nullopt},
            {"ofType", [ofType = typeRef.ofType, &doc]() {
                return ofType != nullptr ? make_optional(CreateTypeRefResolver(*ofType, doc)) : nullopt;
            }}
        };
    }
    // NamedType: resolve actual kind
    const auto kind = resolveKind(typeRef.name, doc);
    return Resolver {
        {"kind", kind},
        {"name", typeRef.name},
        {"ofType", monostate{}}
    };
}

Resolver CreateInputValueResolver(const InputValueDefinition& input, const Document& doc) {
    return Resolver {
        {"name", input.name},
        {"description", input.description},
        {"type", [input, &doc]() { return CreateTypeRefResolver(input.type, doc); }},
        {"defaultValue", input.defaultValue}
    };
}

Resolver CreateEnumValueResolver(const EnumValueDefinition& enumValue) {
    return Resolver {
        {"name", enumValue.name},
        {"description", enumValue.description},
        {"isDeprecated", enumValue.isDeprecated},
        {"deprecationReason", enumValue.deprecationReason}
    };
}

Resolver CreateFieldResolver(const FieldDefinition& field, const Document& doc) {
    return Resolver {
        {"name", field.name},
        {"description", field.description},
        {"type", CreateTypeRefResolver(field.type, doc)},
        {"isDeprecated", field.isDeprecated},
        {"deprecationReason", field.deprecationReason},
        {"args", [args = field.args, &doc]() {
            return to_vector(args | views::transform([&doc](const auto& a) -> ValueResolver {
                return CreateInputValueResolver(a, doc);
            }));
        }}
    };
}

Resolver CreateDirectiveResolver(const DirectiveDefinition& directive, const Document& doc) {
    return Resolver {
        {"name", directive.name},
        {"description", directive.description},
        {"isRepeatable", directive.isRepeatable},
        {"locations", [locations = directive.locations]() {
            return to_vector(locations | views::transform([](const auto& location) -> ValueResolver {
                return location._to_string();
            }));
        }},
        {"args", [args = directive.args, &doc]() {
            return to_vector(args | views::transform([&doc](const auto& a) -> ValueResolver {
                return CreateInputValueResolver(a, doc);
            }));
        }}
    };
}

Resolver CreateTypeResolver(const TypeDefinition& type, const Document& doc) {
    return Resolver {
        {"kind", string(type.kind._to_string())},
        {"name", type.name},
        {"description", type.description},
        {"fields", [type, &doc]() -> ValueResolver {
            switch (type.kind._value) {
                case TypeKind::OBJECT:
                case TypeKind::INTERFACE: {
                    return to_vector(type.fields | views::transform([&doc](const auto& field) -> ValueResolver {
                        return CreateFieldResolver(field, doc);
                    }));
                }
                default:
                    return monostate{};
            }
        }},
        {"interfaces", [type, &doc]() -> ValueResolver {
            if (type.kind._value == TypeKind::OBJECT) {
                return to_vector(type.interfaces | views::transform([&doc](const auto& interface) -> ValueResolver {
                    return CreateTypeRefResolver(TypeRef::Named(interface), doc);
                }));
            }
            if (type.kind._value == TypeKind::INTERFACE) {
                return vector<ValueResolver>{};
            }
            return monostate{};
        }},
        {"possibleTypes", [type, &doc]() -> ValueResolver {
            switch (type.kind._value) {
                case TypeKind::INTERFACE: {
                    return to_vector(type.possibleTypes | views::transform([&doc](const auto& pt) -> ValueResolver {
                        return CreateTypeRefResolver(TypeRef::Named(pt), doc);
                    }));
                }
                case TypeKind::UNION: {
                    return to_vector(type.unionTypes | views::transform([&doc](const auto& ut) -> ValueResolver {
                        return CreateTypeRefResolver(TypeRef::Named(ut), doc);
                    }));
                }
                default:
                    return monostate{};
            }
        }},
        {"enumValues", [type]() -> ValueResolver {
            if (type.kind._value != TypeKind::ENUM)
                return monostate{};

            return to_vector(type.enumValues | views::transform([](const auto& enumValue) -> ValueResolver {
                return CreateEnumValueResolver(enumValue);
            }));
        }},
        {"inputFields", [type, &doc]() -> ValueResolver {
            if (type.kind._value != TypeKind::INPUT_OBJECT)
                return monostate{};

            return to_vector(type.inputFields | views::transform([&doc](const auto& field) -> ValueResolver {
                return CreateInputValueResolver(field, doc);
            }));
        }}
    };
}

Resolver CreateNamedTypeRef(const string& name) {
    return Resolver{
        {"name", name},
        {"kind", string("OBJECT")}
    };
}

static InputValueDefinition makeIncludeDeprecatedArg() {
    return InputValueDefinition{
        .name = "includeDeprecated",
        .description = nullopt,
        .type = _Boolean,
        .defaultValue = "false"
    };
}

static TypeDefinition BuiltInScalar(const string& name, const string& desc) {
    return TypeDefinition{
        .kind = TypeKind::SCALAR,
        .name = name,
        .description = desc
    };
}

static vector<TypeDefinition> BuiltinScalars() {
    return {
        BuiltInScalar("String",
            "The `String` scalar type represents textual data, represented as UTF-8 character sequences. The String type is most often used by GraphQL to represent free-form human-readable text."),
        BuiltInScalar("Int",
            "The `Int` scalar type represents non-fractional signed whole numeric values. Int can represent values between -(2^31) and 2^31 - 1."),
        BuiltInScalar("Boolean",
            "The `Boolean` scalar type represents `true` or `false`."),
        BuiltInScalar("Float",
            "The `Float` scalar type represents signed double-precision fractional values as specified by [IEEE 754](https://en.wikipedia.org/wiki/IEEE_floating_point)."),
        BuiltInScalar("ID",
            "The `ID` scalar type represents a unique identifier, often used to refetch an object or as key for a cache. The ID type appears in a JSON response as a String; however, it is not intended to be human-readable. When expected as an input type, any string (such as `\"4\"`) or integer (such as `4`) input value will be accepted as an ID.")
    };
}

static void collectTypeName(
    const string& typeName,
    unordered_set<string>& visited,
    vector<string>& result,
    const Document& doc
) {
    if (visited.count(typeName)) return;
    visited.insert(typeName);
    result.push_back(typeName);

    if (!doc.types.count(typeName)) return; // built-in or unknown: no further DFS
    const auto& type = doc.types.at(typeName);

    auto visitRef = [&](const TypeRef& ref) {
        collectTypeName(getNamedTypeName(ref), visited, result, doc);
    };

    switch (type.kind._value) {
        case TypeKind::OBJECT:
        case TypeKind::INTERFACE:
            for (const auto& iface : type.interfaces) collectTypeName(iface, visited, result, doc);
            for (const auto& field : type.fields) {
                visitRef(field.type);
                for (const auto& arg : field.args) visitRef(arg.type);
            }
            break;
        case TypeKind::UNION:
            for (const auto& ut : type.unionTypes) collectTypeName(ut, visited, result, doc);
            break;
        case TypeKind::INPUT_OBJECT:
            for (const auto& f : type.inputFields) visitRef(f.type);
            break;
        default:
            break;
    }
}

static vector<string> buildTypeOrder(const Document& doc) {
    unordered_set<string> visited(doc.typeOrder.begin(), doc.typeOrder.end());
    vector<string> result;

    for (const auto& name : doc.typeOrder) {
        visited.erase(name); // unmark so DFS will process it
        collectTypeName(name, visited, result, doc);
    }

    static const vector<string> introspectionOrder = {
        "__Schema",
        "__Type",
        "__TypeKind",
        "__Field",
        "__InputValue",
        "__EnumValue",
        "__Directive",
        "__DirectiveLocation"
    };
    for (const auto& name : introspectionOrder) {
        if (!visited.count(name)) {
            visited.insert(name);
            result.push_back(name);
        }
    }

    return result;
}

Resolver CreateSchemaResolver(const Document& doc) {
    auto builtinScalarDefs = BuiltinScalars();
    unordered_map<string, TypeDefinition> builtinScalarsMap;
    for (auto& s : builtinScalarDefs) builtinScalarsMap[s.name] = s;

    static const vector<TypeDefinition> introspectionTypeDefs = {
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
                    .args = {makeIncludeDeprecatedArg()}
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
                    .args = {makeIncludeDeprecatedArg()}
                },
                FieldDefinition {
                    .name = "inputFields",
                    .type = TypeRef::ListNonNull(__InputValue),
                    .args = {makeIncludeDeprecatedArg()}
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
                    .args = {makeIncludeDeprecatedArg()}
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
                    .args = {makeIncludeDeprecatedArg()}
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
    unordered_map<string, TypeDefinition> introspectionTypeMap;
    for (const auto& t : introspectionTypeDefs) introspectionTypeMap[t.name] = t;

    auto getTypeDef = [&](const string& name) -> const TypeDefinition* {
        if (doc.types.count(name)) return &doc.types.at(name);
        if (builtinScalarsMap.count(name)) return &builtinScalarsMap.at(name);
        if (introspectionTypeMap.count(name)) return &introspectionTypeMap.at(name);
        return nullptr;
    };

    auto orderedNames = buildTypeOrder(doc);

    // Build ordered type definitions
    vector<TypeDefinition> orderedTypes;
    orderedTypes.reserve(orderedNames.size());
    for (const auto& name : orderedNames) {
        if (auto* td = getTypeDef(name)) {
            orderedTypes.push_back(*td);
        }
    }

    vector<DirectiveDefinition> allDirectives = doc.directives;
    auto builtinDirs = {
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
    allDirectives.insert(allDirectives.end(), builtinDirs.begin(), builtinDirs.end());

    return Resolver {
        {"queryType", and_then(doc.queryTypeName, [](const auto& name) {
            return CreateNamedTypeRef(name);
        })},
        {"mutationType", and_then(doc.mutationTypeName, [](const auto& name) {
            return CreateNamedTypeRef(name);
        })},
        {"subscriptionType", and_then(doc.subscriptionTypeName, [](const auto& name) {
            return CreateNamedTypeRef(name);
        })},
        {"directives", [allDirectives, &doc]() {
            return to_vector(allDirectives | views::transform([&doc](const auto& d) -> ValueResolver {
                return CreateDirectiveResolver(d, doc);
            }));
        }},
        {"types", [orderedTypes, &doc]() {
            return to_vector(orderedTypes | views::transform([&doc](const auto& type) -> ValueResolver {
                return CreateTypeResolver(type, doc);
            }));
        }}
    };
}

}
