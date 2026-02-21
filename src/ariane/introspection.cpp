#include <ariane/introspection.h>

using namespace ariane::graphql;

namespace ariane::graphql {

Resolver CreateTypeRefResolver(const TypeRef& typeRef) {
    Resolver resolver;
    resolver["kind"] = typeRef.kind._to_string();

    if (!typeRef.name.empty()) {
        resolver["name"] = typeRef.name;
    } else {
        resolver["name"] = std::monostate{};
    }

    if (typeRef.ofType) {
        resolver["ofType"] = [ofType = *typeRef.ofType]() { return CreateTypeRefResolver(ofType); };
    } else {
        resolver["ofType"] = std::monostate{};
    }

    return resolver;
}

Resolver CreateInputValueResolver(const InputValueDefinition& input) {
    Resolver resolver;
    resolver["name"] = input.name;
    resolver["description"] = input.description.value_or("");
    resolver["type"] = [type = input.type]() { return CreateTypeRefResolver(type); };
    resolver["defaultValue"] = input.defaultValue.value_or(std::string());
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
        std::vector<ValueResolver> argsResolvers;
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
        std::vector<ValueResolver> locationResolvers;
        for (const auto& location : locations) {
            locationResolvers.push_back(DirectiveDefinition::to_string(location));
        }
        return ValueResolver(locationResolvers);
    };

    resolver["args"] = [args = directive.args]() {
        std::vector<ValueResolver> argsResolvers;
        for (const auto& arg : args) {
            argsResolvers.push_back(CreateInputValueResolver(arg));
        }
        return ValueResolver(argsResolvers);
    };

    return resolver;
}

Resolver CreateTypeResolver(const TypeDefinition& type) {
    Resolver resolver;
    resolver["kind"] = type.kind._to_string();
    resolver["name"] = type.name;
    resolver["description"] = type.description.value_or("");

    resolver["fields"] = [type]() -> ValueResolver {
        if (type.kind._value != TypeKind::OBJECT && type.kind._value != TypeKind::INTERFACE) {
            return std::monostate{};
        }
        std::vector<ValueResolver> fieldResolvers;
        for (const auto& field : type.fields) {
            fieldResolvers.push_back(CreateFieldResolver(field));
        }
        return ValueResolver(fieldResolvers);
    };

    resolver["interfaces"] = [type]() -> ValueResolver {
        if (type.kind._value != TypeKind::OBJECT) {
            return std::monostate{};
        }
        std::vector<ValueResolver> interfaceResolvers;
        for (const auto& iface : type.interfaces) {
            Resolver ifaceRef;
            ifaceRef["kind"] = "NAMED_TYPE";
            ifaceRef["name"] = iface;
            ifaceRef["ofType"] = std::monostate{};
            interfaceResolvers.push_back(ifaceRef);
        }
        return ValueResolver(interfaceResolvers);
    };

    resolver["possibleTypes"] = [type]() -> ValueResolver {
        if (type.kind._value != TypeKind::INTERFACE && type.kind._value != TypeKind::UNION) {
            return std::monostate{};
        }
        std::vector<ValueResolver> possibleTypeResolvers;
        const auto& types = type.kind._value == TypeKind::INTERFACE ? type.possibleTypes : type.unionTypes;
        for (const auto& possibleType : types) {
            Resolver typeRef;
            typeRef["kind"] = "NAMED_TYPE";
            typeRef["name"] = possibleType;
            typeRef["ofType"] = std::monostate{};
            possibleTypeResolvers.push_back(typeRef);
        }
        return ValueResolver(possibleTypeResolvers);
    };

    resolver["enumValues"] = [type]() -> ValueResolver {
        if (type.kind._value != TypeKind::ENUM) {
            return std::monostate{};
        }
        std::vector<ValueResolver> enumValueResolvers;
        for (const auto& enumValue : type.enumValues) {
            enumValueResolvers.push_back(CreateEnumValueResolver(enumValue));
        }
        return ValueResolver(enumValueResolvers);
    };

    resolver["inputFields"] = [type]() -> ValueResolver {
        if (type.kind._value != TypeKind::INPUT_OBJECT) {
            return std::monostate{};
        }
        std::vector<ValueResolver> inputFieldResolvers;
        for (const auto& inputField : type.inputFields) {
            inputFieldResolvers.push_back(CreateInputValueResolver(inputField));
        }
        return ValueResolver(inputFieldResolvers);
    };

    return resolver;
}

Resolver CreateSchemaResolver(const Document& schema) {
    Resolver resolver;

    resolver["queryType"] =
         schema.queryTypeName.has_value() ? ValueResolver(*schema.queryTypeName) : ValueResolver(std::monostate{});
    resolver["mutationType"] = schema.mutationTypeName.has_value() ? ValueResolver(*schema.mutationTypeName)
                                                                   : ValueResolver(std::monostate{});
    resolver["subscriptionType"] = schema.subscriptionTypeName.has_value() ? ValueResolver(*schema.subscriptionTypeName)
                                                                           : ValueResolver(std::monostate{});

    resolver["types"] = [types = schema.types]() {
        std::vector<ValueResolver> typeResolvers;
        for (const auto& [name, type] : types) {
            typeResolvers.push_back(CreateTypeResolver(type));
        }
        return ValueResolver(typeResolvers);
    };

    resolver["directives"] = [directives = schema.directives]() {
        std::vector<ValueResolver> directiveResolvers;
        for (const auto& directive : directives) {
            directiveResolvers.push_back(CreateDirectiveResolver(directive));
        }
        return ValueResolver(directiveResolvers);
    };

    return resolver;
}

}
