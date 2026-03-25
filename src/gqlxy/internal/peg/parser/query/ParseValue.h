#pragma once

#include <graphqlservice/internal/SyntaxTree.h>

namespace gqlxy::internal {

std::optional<std::string> ParseValue(const ::graphql::peg::ast_node& node);

}
