#pragma once

#include <ariane/internal/ast/Selection.h>
#include <graphqlservice/internal/SyntaxTree.h>

namespace ariane::graphql::internal {

InlineFragment ParseInlineFragment(const ::graphql::peg::ast_node& node);

}