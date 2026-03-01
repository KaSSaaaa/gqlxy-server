#pragma once

#include <ariane/internal/ast/Argument.h>
#include <graphqlservice/internal/SyntaxTree.h>
#include <vector>

namespace ariane::graphql::internal {

std::vector<Argument> ParseArguments(const ::graphql::peg::ast_node& node);

}