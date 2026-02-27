#include <ariane/internal/introspection/types/TypeRef.h>

using namespace std;

namespace ariane::graphql::internal {

std::string TypeRef::typeName() const {
    return ofType != nullptr ? ofType->typeName() : name;
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
