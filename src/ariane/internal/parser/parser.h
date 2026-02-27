#pragma once

#include <ariane/internal/introspection/types/EnumValueDefinition.h>
#include <ariane/internal/introspection/types/FieldDefinition.h>
#include <ariane/internal/introspection/types/InputValueDefinition.h>
#include <ariane/internal/introspection/types/TypeDefinition.h>
#include <ariane/internal/introspection/types/TypeRef.h>

#include <memory>

namespace graphql::peg {
class ast_node;
}

namespace ariane::graphql::internal {
struct Document;

std::shared_ptr<Document> ParseTypeDefs(const std::string& typeDefs);
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