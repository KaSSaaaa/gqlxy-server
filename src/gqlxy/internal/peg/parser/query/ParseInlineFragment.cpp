#include "ParseInlineFragment.h"

#include <gqlxy/internal/peg/first_node.h>
#include <gqlxy/internal/utils/optional.h>
#include <graphqlservice/internal/Grammar.h>

#include "ParseDirectives.h"
#include "ParseSelectionSet.h"

using namespace std;
using namespace graphql;

namespace gqlxy::internal {

InlineFragment ParseInlineFragment(const peg::ast_node& node)  {
    return InlineFragment {
        .typeCondition = and_then(first_node<peg::type_condition>(node), [](const auto* tc) {
            return and_then(first_node<peg::named_type>(*tc), [](const auto* n) {
                return make_optional(n->string());
            });
        }),
        .directives = ParseDirectives(node),
        .selectionSet = and_then(first_node<peg::selection_set>(node), [](const auto* n) {
            return make_optional(ParseSelectionSet(*n));
        }),
    };
}

}