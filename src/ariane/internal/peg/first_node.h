#pragma once

#include <functional>
#include <optional>

#include <graphqlservice/internal/SyntaxTree.h>

namespace ariane::graphql::internal {

template <typename T>
std::optional<::graphql::peg::ast_node*> first_node(
    const ::graphql::peg::ast_node& node,
    const std::function<bool(const ::graphql::peg::ast_node&)>& predicate = [](const auto&) { return true; }
) {
    for (const auto& child : node.children) {
        if (child && child->is_type<T>() && predicate(*child)) {
            return child.get();
        }
    }
    return std::nullopt;
}

template <typename T>
std::optional<::graphql::peg::ast_node*> find_node(const ::graphql::peg::ast_node& node) {
    for (const auto& child : node.children) {
        if (!child) continue;
        if (child->is_type<T>()) return child.get();
        if (auto found = find_node<T>(*child)) return found;
    }
    return std::nullopt;
}

}