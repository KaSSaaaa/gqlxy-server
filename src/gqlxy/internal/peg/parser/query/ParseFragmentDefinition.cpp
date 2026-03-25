#include "ParseFragmentDefinition.h"

#include <gqlxy/internal/peg/first_node.h>
#include <gqlxy/internal/utils/optional.h>
#include <graphqlservice/internal/Grammar.h>
#include <gqlxy/internal/ast/SelectionSet.h>
#include <gqlxy/internal/ast/Selection.h>

#include "ParseSelectionSet.h"

using namespace std;
using namespace graphql;

namespace gqlxy::internal {

FragmentDefinition ParseFragmentDefinition(const peg::ast_node& node) {
    return FragmentDefinition {
        .name = and_then(first_node<peg::fragment_name>(node), [](const auto* n) {
            return n->string();
        }),
        .typeCondition = and_then(first_node<peg::type_condition>(node), [](const auto* tc) {
            return and_then(first_node<peg::named_type>(*tc), [](const auto* n) {
                return make_optional(n->string());
            });
        }).value_or(""),
        .selectionSet = and_then(first_node<peg::selection_set>(node), [](const auto* ss) {
            return make_optional(ParseSelectionSet(*ss));
        }).value_or(SelectionSet{}),
    };
}

}