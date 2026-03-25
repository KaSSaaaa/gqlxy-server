#pragma once

#include <gqlxy/internal/ast/FragmentDefinition.h>
#include <graphqlservice/internal/SyntaxTree.h>

namespace gqlxy::internal {

FragmentDefinition ParseFragmentDefinition(const ::graphql::peg::ast_node& node);

}
