#pragma once

#include <ariane/internal/ast/OperationDefinition.h>
#include <graphqlservice/internal/SyntaxTree.h>

namespace ariane::graphql::internal {

OperationDefinition ParseOperationDefinition(const ::graphql::peg::ast_node& node);

}