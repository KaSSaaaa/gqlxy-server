#pragma once

#include <graphqlservice/internal/SyntaxTree.h>

namespace ariane::graphql::internal {

std::string ParseArgumentValue(const ::graphql::peg::ast_node& node);

}
