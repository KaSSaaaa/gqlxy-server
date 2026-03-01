#pragma once

#include <ariane/internal/ast/SelectionSet.h>
#include <graphqlservice/internal/SyntaxTree.h>

namespace ariane::graphql::internal {

SelectionSet ParseSelectionSet(const ::graphql::peg::ast_node& node);

}