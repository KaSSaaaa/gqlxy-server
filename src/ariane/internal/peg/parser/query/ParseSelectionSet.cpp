#include "ParseSelectionSet.h"

#include <ariane/internal/peg/is_type.h>
#include <ariane/internal/peg/parser/query/ParseSelection.h>
#include <ariane/internal/utils/ranges.h>
#include <graphqlservice/internal/Grammar.h>
#include <graphqlservice/internal/SyntaxTree.h>

using namespace std;
using namespace graphql;

namespace ariane::graphql::internal {

SelectionSet ParseSelectionSet(const peg::ast_node& node) {
    return SelectionSet {
        .selections = to_vector(node.children
            | views::filter(is_type<peg::field, peg::fragment_spread, peg::inline_fragment>())
            | views::transform([](const auto& child) {
                return ParseSelection(*child);
            }))
    };
}

}