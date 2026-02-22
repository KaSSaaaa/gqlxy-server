#include <ariane/internal/introspection/types/TypeRef.h>

using namespace std;

namespace ariane::graphql::internal {

TypeRef TypeRef::Named(const string& typeName) {
    TypeRef ref;
    ref.kind = TypeRefKind::NamedType;
    ref.name = typeName;
    ref.ofType = nullptr;
    return ref;
}

TypeRef TypeRef::NonNull(TypeRef inner) {
    TypeRef ref;
    ref.kind = TypeRefKind::NonNull;
    ref.name = "";
    ref.ofType = make_unique<TypeRef>(std::move(inner));
    return ref;
}

TypeRef TypeRef::List(TypeRef inner) {
    TypeRef ref;
    ref.kind = TypeRefKind::List;
    ref.name = "";
    ref.ofType = make_unique<TypeRef>(std::move(inner));
    return ref;
}

}
