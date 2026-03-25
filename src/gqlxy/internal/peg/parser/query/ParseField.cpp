#include "ParseField.h"

#include <gqlxy/internal/peg/first_node.h>
#include <gqlxy/internal/utils/optional.h>
#include <graphqlservice/internal/Grammar.h>

#include "ParseArguments.h"
#include "ParseDirectives.h"
#include "ParseSelectionSet.h"

using namespace std;
using namespace graphql;

namespace gqlxy::internal {

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
            return make_optional(ParseSelectionSet(*n));
        }),
   };
}

}