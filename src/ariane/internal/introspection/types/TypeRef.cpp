#include "TypeRef.h"

using namespace std;

namespace ariane::graphql::internal {

string TypeRef::TypeName() const {
    return ofType != nullptr ? ofType->TypeName() : name;
}

string TypeRef::ToString() const {
    if (kind._value == TypeRefKind::LIST)
        return "[" + ofType->ToString() + "]";
    if (kind._value == TypeRefKind::NON_NULL)
        return ofType->ToString() + "!";
    return name;
}

TypeRef TypeRef::Named(const string& typeName) {
    return TypeRef {
        .kind = TypeRefKind::NamedType,
        .name = typeName,
    };
}

TypeRef TypeRef::NonNull(TypeRef inner) {
    return TypeRef {
        .kind = TypeRefKind::NON_NULL,
        .name = "",
        .ofType = make_shared<TypeRef>(std::move(inner)),
    };
}

TypeRef TypeRef::NonNullList(TypeRef inner) {
    return NonNull(List(std::move(inner)));
}

TypeRef TypeRef::List(TypeRef inner) {
    return TypeRef {
        .kind = TypeRefKind::LIST,
        .name = "",
        .ofType = make_shared<TypeRef>(std::move(inner)),
    };
}

TypeRef TypeRef::ListNonNull(TypeRef inner) {
    return List(NonNull(std::move(inner)));
}

TypeRef TypeRef::NonNullListNonNull(TypeRef inner) {
    return NonNull(ListNonNull(std::move(inner)));
}

}
