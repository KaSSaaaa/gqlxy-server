#pragma once

#include <gqlxy/internal/introspection/types/EnumValueDefinition.h>
#include <gqlxy/internal/introspection/types/FieldDefinition.h>
#include <gqlxy/internal/introspection/types/InputValueDefinition.h>
#include <gqlxy/internal/introspection/types/TypeDefinition.h>
#include <gqlxy/internal/introspection/types/TypeRef.h>

#include <memory>

namespace graphql::peg {
class ast_node;
}

namespace gqlxy::internal {
struct SchemaDefinition;

std::shared_ptr<SchemaDefinition> ParseSchemaDefinition(const std::string& typeDefs);
TypeRef ParseTypeRef(const ::graphql::peg::ast_node& node);
InputValueDefinition ParseInputValue(const ::graphql::peg::ast_node& node);
EnumValueDefinition ParseEnumValue(const ::graphql::peg::ast_node& node);
TypeDefinition ParseType(const ::graphql::peg::ast_node& node, const TypeKind& kind);
FieldDefinition ParseField(const ::graphql::peg::ast_node& node);
std::vector<FieldDefinition> ParseFields(const std::optional<::graphql::peg::ast_node*>& node);
TypeDefinition ParseObjectType(const ::graphql::peg::ast_node& node);
TypeDefinition ParseInterfaceType(const ::graphql::peg::ast_node& node);
std::optional<TypeDefinition> ParseType(const ::graphql::peg::ast_node& node);

}