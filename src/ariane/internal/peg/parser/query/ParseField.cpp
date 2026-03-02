#include "ParseField.h"

#include <ariane/internal/peg/first_node.h>
#include <ariane/internal/utils/optional.h>
#include <graphqlservice/internal/Grammar.h>

#include "ParseArguments.h"
#include "ParseDirectives.h"
#include "ParseSelectionSet.h"

using namespace std;
using namespace graphql;

namespace ariane::graphql::internal {

Field ParseSelectionField(const peg::ast_node& node) {
    return Field{
        .alias = and_then(first_node<peg::alias_name>(node), [](const auto* n) {
            return make_optional(n->string());
        }),
        .name = and_then(first_node<peg::field_name>(node), [](const auto* n) {
            return n->string();
        }),
        .arguments = ParseArguments(node),
        .directives = ParseDirectives(node),
        .selectionSet = and_then(first_node<peg::selection_set>(node), [](const auto* n) {
            return make_optional(make_shared<SelectionSet>(ParseSelectionSet(*n)));
        }).value_or(nullptr),
   };
}

}