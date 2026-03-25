#pragma once

#include <gqlxy/internal/ast/Directive.h>
#include <graphqlservice/internal/SyntaxTree.h>

#include <vector>

namespace gqlxy::internal {

std::vector<Directive> ParseDirectives(const ::graphql::peg::ast_node& node);

}
