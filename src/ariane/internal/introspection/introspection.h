#pragma once

#include <ariane/resolvers.h>
#include <ariane/schema.h>

namespace ariane::graphql {
namespace internal {
struct Document;
struct DirectiveDefinition;
struct EnumValueDefinition;
struct FieldDefinition;
struct InputValueDefinition;
struct TypeDefinition;
struct TypeRef;
}

Resolver CreateSchemaResolver(const internal::Document& schema);
Resolver CreateTypeResolver(const internal::TypeDefinition& type);
Resolver CreateFieldResolver(const internal::FieldDefinition& field);
Resolver CreateInputValueResolver(const internal::InputValueDefinition& input);
Resolver CreateEnumValueResolver(const internal::EnumValueDefinition& enumValue);
Resolver CreateTypeRefResolver(const internal::TypeRef& typeRef);
Resolver CreateDirectiveResolver(const internal::DirectiveDefinition& directive);

}
