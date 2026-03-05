#include "ParseSelection.h"

#include <graphqlservice/internal/Grammar.h>
#include <ariane/internal/ast/Selection.h>

#include "ParseField.h"
#include "ParseFragmentSpread.h"
#include "ParseInlineFragment.h"

using namespace std;
using namespace graphql;

namespace ariane::graphql::internal {

Selection ParseSelection(const peg::ast_node& node) {
    if (node.is_type<peg::field>())
        return { ParseSelectionField(node) };
    if (node.is_type<peg::fragment_spread>())
        return { ParseFragmentSpread(node) };
    return { ParseInlineFragment(node) };
}

}