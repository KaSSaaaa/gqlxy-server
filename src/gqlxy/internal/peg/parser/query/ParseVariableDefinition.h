#pragma once

#include <graphqlservice/internal/SyntaxTree.h>
#include <gqlxy/internal/ast/VariableDefinition.h>

namespace gqlxy::internal {

VariableDefinition ParseVariableDefinition(const ::graphql::peg::ast_node& node);

}