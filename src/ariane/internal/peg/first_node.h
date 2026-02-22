#pragma once

#include <graphqlservice/internal/SyntaxTree.h>

#include <optional>

namespace ariane::graphql::internal {

template <typename T>
std::optional<::graphql::peg::ast_node*> first_node(const ::graphql::peg::ast_node& node) {
    for (const auto& child : node.children) {
        if (child && child->is_type<T>()) {
            return child.get();
        }
    }
    return std::nullopt;
}

}