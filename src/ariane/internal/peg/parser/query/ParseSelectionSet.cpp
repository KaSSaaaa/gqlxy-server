#include "ParseSelectionSet.h"

#include <graphqlservice/internal/Grammar.h>
#include <graphqlservice/internal/SyntaxTree.h>

#include "ParseSelection.h"

using namespace std;
using namespace graphql;

namespace ariane::graphql::internal {

SelectionSet ParseSelectionSet(const peg::ast_node& node) {
    SelectionSet result;
    for (const auto& child : node.children) {
        if (!child) continue;
        if (child->is_type<peg::field>() ||
            child->is_type<peg::fragment_spread>() ||
            child->is_type<peg::inline_fragment>())
            result.selections.push_back(ParseSelection(*child));
    }
    return result;
}

}