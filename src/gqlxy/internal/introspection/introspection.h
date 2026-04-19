#pragma once

#include <gqlxy/resolvers.h>
#include <gqlxy/schema.h>
#include <gqlxy/parser/introspection/types/type_ref.h>

namespace gqlxy::internal {
struct SchemaDefinition;
struct DirectiveDefinition;
struct EnumValueDefinition;
struct FieldDefinition;
struct InputValueDefinition;
struct TypeDefinition;

Resolver CreateSchemaResolver(const SchemaDefinition& schema);
Resolver CreateTypeResolver(const TypeDefinition& type, const SchemaDefinition& schema);
Resolver CreateFieldResolver(const FieldDefinition& field, const SchemaDefinition& schema);
Resolver CreateInputValueResolver(const InputValueDefinition& input, const SchemaDefinition& schema);
Resolver CreateEnumValueResolver(const EnumValueDefinition& enumValue);
Resolver CreateTypeRefResolver(const parser::TypeRef& typeRef, const SchemaDefinition& schema);
Resolver CreateDirectiveResolver(const DirectiveDefinition& directive, const SchemaDefinition& schema);

std::optional<TypeDefinition> GetTypeDefinition(const SchemaDefinition& schema, const std::string& name);

}
