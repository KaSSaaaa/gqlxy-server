#pragma once

#include <ariane/internal/ast/Directive.h>
#include <graphqlservice/internal/SyntaxTree.h>

#include <vector>

namespace ariane::graphql::internal {

std::vector<Directive> ParseDirectives(const ::graphql::peg::ast_node& node);

}
