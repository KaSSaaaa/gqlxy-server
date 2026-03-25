#pragma once

#include <gqlxy/internal/ast/Argument.h>
#include <graphqlservice/internal/SyntaxTree.h>
#include <vector>

namespace gqlxy::internal {

std::vector<Argument> ParseArguments(const ::graphql::peg::ast_node& node);

}