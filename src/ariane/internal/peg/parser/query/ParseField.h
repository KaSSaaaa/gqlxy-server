#pragma once

#include <ariane/internal/ast/Selection.h>
#include <graphqlservice/internal/SyntaxTree.h>

namespace ariane::graphql::internal {

Field ParseSelectionField(const ::graphql::peg::ast_node& node);

}