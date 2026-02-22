#pragma once
#include <vector>

#include "first_node.h"

namespace ariane::graphql::internal {

template <typename TNode, typename TResult>
std::vector<TResult> transform_children(const ::graphql::peg::ast_node& node,
                                        const std::function<TResult(const ::graphql::peg::ast_node&)>& transform) {
    std::vector<TResult> result;
    for_each_child<TNode>(node, [transform, &result](const ::graphql::peg::ast_node& child) {
        result.emplace_back(transform(child));
    });
    return result;
}

}