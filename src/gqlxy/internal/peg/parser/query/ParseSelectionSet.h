#pragma once

#include <gqlxy/internal/ast/SelectionSet.h>
#include <graphqlservice/internal/SyntaxTree.h>

namespace gqlxy::internal {

SelectionSet ParseSelectionSet(const ::graphql::peg::ast_node& node);

}