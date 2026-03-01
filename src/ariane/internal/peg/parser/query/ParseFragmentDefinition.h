#pragma once

#include <ariane/internal/ast/FragmentDefinition.h>
#include <graphqlservice/internal/SyntaxTree.h>

namespace ariane::graphql::internal {

FragmentDefinition ParseFragmentDefinition(const ::graphql::peg::ast_node& node);

}
