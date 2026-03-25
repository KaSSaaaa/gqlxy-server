#pragma once

#include <gqlxy/internal/ast/Selection.h>
#include <graphqlservice/internal/SyntaxTree.h>

namespace gqlxy::internal {

InlineFragment ParseInlineFragment(const ::graphql::peg::ast_node& node);

}