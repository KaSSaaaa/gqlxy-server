#pragma once

#include <graphqlservice/internal/SyntaxTree.h>

namespace ariane::graphql::internal {

template <typename... TTypes>
bool is_type(const ::graphql::peg::ast_node& node) {
    return (... || node.is_type<TTypes>());
}

template <typename... TTypes>
bool is_type(const std::unique_ptr<::graphql::peg::ast_node>& node) {
    return is_type<TTypes...>(*node);
}

template <typename... TTypes>
auto is_type() {
    return [](const auto& node) { return is_type<TTypes...>(node); };
}

}