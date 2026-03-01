#pragma once

#include <graphqlservice/internal/SyntaxTree.h>
#include <ariane/internal/ast/VariableDefinition.h>

namespace ariane::graphql::internal {

VariableDefinition ParseVariableDefinition(const ::graphql::peg::ast_node& node);

}