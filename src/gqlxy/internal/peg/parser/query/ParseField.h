#pragma once

#include <gqlxy/internal/ast/Selection.h>
#include <graphqlservice/internal/SyntaxTree.h>

namespace gqlxy::internal {

Field ParseSelectionField(const ::graphql::peg::ast_node& node);

}