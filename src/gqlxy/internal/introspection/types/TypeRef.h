#pragma once

#include <better-enums/enum.h>

#include <memory>
#include <string>

namespace gqlxy::internal {

BETTER_ENUM(TypeRefKind, int, NamedType, NON_NULL, LIST);

struct TypeRef {
    TypeRefKind kind;
    std::string name;
    std::shared_ptr<TypeRef> ofType;

    std::string TypeName() const;
    std::string ToString() const;

    static TypeRef Named(const std::string& typeName);
    static TypeRef NonNull(TypeRef inner);
    static TypeRef NonNullList(TypeRef inner);
    static TypeRef List(TypeRef inner);
    static TypeRef ListNonNull(TypeRef inner);
    static TypeRef NonNullListNonNull(TypeRef inner);
};

}
