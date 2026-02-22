#include "introspection.h"
#include <ariane/internal/introspection/types/Document.h>

using namespace std;
using namespace ariane::graphql;
using namespace ariane::graphql::internal;

namespace ariane::graphql {

Resolver CreateTypeRefResolver(const TypeRef& typeRef) {
    Resolver resolver;
    resolver["kind"] = typeRef.kind._to_string();

    if (!typeRef.name.empty()) {
        resolver["name"] = typeRef.name;
    } else {
        resolver["name"] = monostate{};
    }

    if (typeRef.ofType) {
        resolver["ofType"] = [ofType = *typeRef.ofType]() { return CreateTypeRefResolver(ofType); };
    } else {
        resolver["ofType"] = monostate{};
    }

    return resolver;
}

Resolver CreateInputValueResolver(const InputValueDefinition& input) {
    Resolver resolver;
    resolver["name"] = input.name;
    resolver["description"] = input.description.value_or("");
    resolver["type"] = [type = input.type]() { return CreateTypeRefResolver(type); };
    resolver["defaultValue"] = input.defaultValue.value_or(string());
    return resolver;
}

Resolver CreateEnumValueResolver(const EnumValueDefinition& enumValue) {
    Resolver resolver;
    resolver["name"] = enumValue.name;
    resolver["description"] = enumValue.description.value_or("");
    resolver["isDeprecated"] = enumValue.isDeprecated;
    resolver["deprecationReason"] = enumValue.deprecationReason.value_or("");
    return resolver;
}

Resolver CreateFieldResolver(const FieldDefinition& field) {
    Resolver resolver;
    resolver["name"] = field.name;
    resolver["description"] = field.description.value_or("");
    resolver["type"] = [type = field.type]() { return CreateTypeRefResolver(type); };
    resolver["isDeprecated"] = field.isDeprecated;
    resolver["deprecationReason"] = field.deprecationReason.value_or("");

    resolver["args"] = [args = field.args]() {
        vector<ValueResolver> argsResolvers;
        for (const auto& arg : args) {
            argsResolvers.push_back(CreateInputValueResolver(arg));
        }
        return ValueResolver(argsResolvers);
    };

    return resolver;
}

Resolver CreateDirectiveResolver(const DirectiveDefinition& directive) {
    Resolver resolver;
    resolver["name"] = directive.name;
    resolver["description"] = directive.description.value_or("");
    resolver["isRepeatable"] = directive.isRepeatable;

    resolver["locations"] = [locations = directive.locations]() {
        vector<ValueResolver> locationResolvers;
        for (const auto& location : locations) {
            locationResolvers.push_back(location._to_string());
        }
        return ValueResolver(locationResolvers);
    };

    resolver["args"] = [args = directive.args]() {
        vector<ValueResolver> argsResolvers;
        for (const auto& arg : args) {
            argsResolvers.push_back(CreateInputValueResolver(arg));
        }
        return ValueResolver(argsResolvers);
    };

    return resolver;
}

Resolver CreateTypeResolver(const TypeDefinition& type) {
    return Resolver {
        {"kind", type.kind._to_string()},
        {"name", type.name},
        {"description", type.description.value_or("")},
        {"fields", [type]() -> ValueResolver {
            if (type.kind._value != TypeKind::OBJECT && type.kind._value != TypeKind::INTERFACE) {
                return monostate{};
            }
            vector<ValueResolver> fieldResolvers;
            for (const auto& field : type.fields) {
                fieldResolvers.push_back(CreateFieldResolver(field));
            }
            return ValueResolver(fieldResolvers);
        }},
        {"interfaces", [type]() -> ValueResolver {
            if (type.kind._value != TypeKind::OBJECT) {
                return monostate{};
            }
            vector<ValueResolver> interfaceResolvers;
            for (const auto& interface : type.interfaces) {
                interfaceResolvers.push_back(Resolver {
                    {"kind", "NAMED_TYPE"},
                    {"name", interface},
                    {"ofType", monostate{}}
                });
            }
            return ValueResolver(interfaceResolvers);
        }},
        {"possibleTypes", [type]() -> ValueResolver {
            if (type.kind._value != TypeKind::INTERFACE && type.kind._value != TypeKind::UNION) {
                return monostate{};
            }
            vector<ValueResolver> possibleTypeResolvers;
            const auto& types = type.kind._value == TypeKind::INTERFACE ? type.possibleTypes : type.unionTypes;
            for (const auto& possibleType : types) {
                possibleTypeResolvers.push_back(Resolver {
                    {"kind", "NAMED_TYPE"},
                    {"name", possibleType},
                    {"ofType", monostate {}}
                });
            }
            return ValueResolver(possibleTypeResolvers);
        }},
        {"enumValues", [type]() -> ValueResolver {
            if (type.kind._value != TypeKind::ENUM) {
                return monostate{};
            }
            vector<ValueResolver> enumValueResolvers;
            for (const auto& enumValue : type.enumValues) {
                enumValueResolvers.push_back(CreateEnumValueResolver(enumValue));
            }
            return ValueResolver(enumValueResolvers);
        }},
        {"inputFields", [type]() -> ValueResolver {
            if (type.kind._value != TypeKind::INPUT_OBJECT) {
                return monostate{};
            }
            vector<ValueResolver> inputFieldResolvers;
            for (const auto& inputField : type.inputFields) {
                inputFieldResolvers.push_back(CreateInputValueResolver(inputField));
            }
            return ValueResolver(inputFieldResolvers);
        }}
    };
}

Resolver CreateSchemaResolver(const Document& schema) {
    Resolver resolver;

    auto makeNamedTypeRef = [](const std::string& name) -> ValueResolver {
        return Resolver{{"name", name}, {"kind", "OBJECT"}};
    };

    resolver["queryType"] = schema.queryTypeName.has_value()
        ? makeNamedTypeRef(*schema.queryTypeName)
        : ValueResolver(monostate{});
    resolver["mutationType"] = schema.mutationTypeName.has_value()
        ? makeNamedTypeRef(*schema.mutationTypeName)
        : ValueResolver(monostate{});
    resolver["subscriptionType"] = schema.subscriptionTypeName.has_value()
        ? makeNamedTypeRef(*schema.subscriptionTypeName)
        : ValueResolver(monostate{});

    resolver["types"] = [types = schema.types]() {
        vector<ValueResolver> typeResolvers;
        for (const auto& [name, type] : types) {
            typeResolvers.push_back(CreateTypeResolver(type));
        }
        return ValueResolver(typeResolvers);
    };

    resolver["directives"] = [directives = schema.directives]() {
        vector<ValueResolver> directiveResolvers;
        for (const auto& directive : directives) {
            directiveResolvers.push_back(CreateDirectiveResolver(directive));
        }
        return ValueResolver(directiveResolvers);
    };

    return resolver;
}

}
