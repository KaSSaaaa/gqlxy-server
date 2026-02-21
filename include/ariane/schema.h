#pragma once

#include <ariane/resolvers.h>
#include <better-enums/enum.h>

#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace ariane::graphql {

BETTER_ENUM(TypeKind, int, SCALAR, OBJECT, INTERFACE, UNION, ENUM, INPUT_OBJECT, LIST, NON_NULL);
BETTER_ENUM(TypeRefKind, int, NamedType, NonNull, List);

struct TypeRef {
    TypeRefKind kind;
    std::string name;
    std::unique_ptr<TypeRef> ofType;

    TypeRef() : kind(TypeRefKind::NamedType), name(""), ofType(nullptr) {}

    TypeRef(const TypeRef& other) : kind(other.kind), name(other.name) {
        if (other.ofType) {
            ofType = std::make_unique<TypeRef>(*other.ofType);
        }
    }

    TypeRef(TypeRef&&) = default;

    TypeRef& operator=(const TypeRef& other) {
        if (this != &other) {
            kind = other.kind;
            name = other.name;
            if (other.ofType) {
                ofType = std::make_unique<TypeRef>(*other.ofType);
            } else {
                ofType = nullptr;
            }
        }
        return *this;
    }

    TypeRef& operator=(TypeRef&&) = default;

    static TypeRef Named(const std::string& typeName);
    static TypeRef NonNull(TypeRef inner);
    static TypeRef List(TypeRef inner);
};

struct InputValueDefinition {
    std::string name;
    std::optional<std::string> description;
    TypeRef type;
    std::optional<std::string> defaultValue;
};

struct EnumValueDefinition {
    std::string name;
    std::optional<std::string> description;
    bool isDeprecated = false;
    std::optional<std::string> deprecationReason;
};

struct DirectiveDefinition {
    enum class Location {
        Query,
        Mutation,
        Subscription,
        Field,
        FragmentDefinition,
        FragmentSpread,
        InlineFragment,
        Schema,
        Scalar,
        Object,
        FieldDefinition,
        ArgumentDefinition,
        Interface,
        Union,
        Enum,
        EnumValue,
        InputObject,
        InputFieldDefinition
    };

    static const char* to_string(Location location) {
        switch (location) {
            case Location::Query:
                return "QUERY";
            case Location::Mutation:
                return "MUTATION";
            case Location::Subscription:
                return "SUBSCRIPTION";
            case Location::Field:
                return "FIELD";
            case Location::FragmentDefinition:
                return "FRAGMENT_DEFINITION";
            case Location::FragmentSpread:
                return "FRAGMENT_SPREAD";
            case Location::InlineFragment:
                return "INLINE_FRAGMENT";
            case Location::Schema:
                return "SCHEMA";
            case Location::Scalar:
                return "SCALAR";
            case Location::Object:
                return "OBJECT";
            case Location::FieldDefinition:
                return "FIELD_DEFINITION";
            case Location::ArgumentDefinition:
                return "ARGUMENT_DEFINITION";
            case Location::Interface:
                return "INTERFACE";
            case Location::Union:
                return "UNION";
            case Location::Enum:
                return "ENUM";
            case Location::EnumValue:
                return "ENUM_VALUE";
            case Location::InputObject:
                return "INPUT_OBJECT";
            case Location::InputFieldDefinition:
                return "INPUT_FIELD_DEFINITION";
        }
        return "FIELD";
    }

    std::string name;
    std::optional<std::string> description;
    std::vector<Location> locations;
    std::vector<InputValueDefinition> args;
    bool isRepeatable = false;
};

struct FieldDefinition {
    std::string name;
    std::optional<std::string> description;
    TypeRef type;
    std::vector<InputValueDefinition> args;
    bool isDeprecated = false;
    std::optional<std::string> deprecationReason;
};

struct TypeDefinition {
    TypeKind kind = TypeKind::OBJECT;
    std::string name;
    std::optional<std::string> description;

    std::vector<FieldDefinition> fields;
    std::vector<std::string> interfaces;
    std::vector<std::string> possibleTypes;
    std::vector<std::string> unionTypes;
    std::vector<EnumValueDefinition> enumValues;
    std::vector<InputValueDefinition> inputFields;
};

struct Document {
    std::unordered_map<std::string, TypeDefinition> types;
    std::vector<DirectiveDefinition> directives;
    std::optional<std::string> queryTypeName;
    std::optional<std::string> mutationTypeName;
    std::optional<std::string> subscriptionTypeName;
};

struct SchemaOptions {
    std::string typeDefs;
    Resolver resolvers;
};

struct ResolveResult {
    std::string data;
    std::string errors;
};

class Schema {
public:
    Schema(const SchemaOptions& options);

    const Document& GetDocument() const { return *document_; }
    const Resolver& GetResolvers() const { return resolvers_; }

    ResolveResult Resolve(const std::string& query, const std::unordered_map<std::string, std::string>& variables = {});

private:
    std::shared_ptr<Document> document_;
    Resolver resolvers_;

    std::shared_ptr<Document> ParseTypeDefs(const std::string& typeDefs);
    void InjectIntrospectionResolvers();
};

}