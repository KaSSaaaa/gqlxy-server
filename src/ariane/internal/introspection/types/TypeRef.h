#pragma once

#include <better-enums/enum.h>

#include <memory>
#include <string>

namespace ariane::graphql::internal {

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
            ofType = other.ofType ? std::make_unique<TypeRef>(*other.ofType) : nullptr;
        }
        return *this;
    }

    TypeRef& operator=(TypeRef&&) = default;

    static TypeRef Named(const std::string& typeName);
    static TypeRef NonNull(TypeRef inner);
    static TypeRef List(TypeRef inner);
};

}
