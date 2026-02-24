#pragma once

#include <ariane/resolvers.h>
#include <ariane/schema.h>

namespace ariane::graphql::internal {
struct Document;
struct DirectiveDefinition;
struct EnumValueDefinition;
struct FieldDefinition;
struct InputValueDefinition;
struct TypeDefinition;
struct TypeRef;

Resolver CreateSchemaResolver(const internal::Document& schema);
Resolver CreateTypeResolver(const internal::TypeDefinition& type, const internal::Document& schema);
Resolver CreateFieldResolver(const internal::FieldDefinition& field, const internal::Document& schema);
Resolver CreateInputValueResolver(const internal::InputValueDefinition& input, const internal::Document& schema);
Resolver CreateEnumValueResolver(const internal::EnumValueDefinition& enumValue);
Resolver CreateTypeRefResolver(const internal::TypeRef& typeRef, const internal::Document& schema);
Resolver CreateDirectiveResolver(const internal::DirectiveDefinition& directive, const internal::Document& schema);

}
