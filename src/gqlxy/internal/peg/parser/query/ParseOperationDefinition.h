#pragma once

#include <gqlxy/internal/ast/OperationDefinition.h>
#include <graphqlservice/internal/SyntaxTree.h>

namespace gqlxy::internal {

OperationDefinition ParseOperationDefinition(const ::graphql::peg::ast_node& node);

}