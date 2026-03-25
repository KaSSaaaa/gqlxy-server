#include "ParseArguments.h"

#include <gqlxy/internal/peg/first_node.h>
#include <gqlxy/internal/peg/is_type.h>
#include <gqlxy/internal/utils/optional.h>
#include <gqlxy/internal/utils/ranges.h>
#include <graphqlservice/internal/Grammar.h>
#include "ParseValue.h"

using namespace std;
using namespace graphql;

namespace gqlxy::internal {

vector<Argument> ParseArguments(const peg::ast_node& node) {
    return and_then(first_node<peg::arguments>(node), [](const auto* argsNode) {
        return make_optional(to_vector(argsNode->children
            | views::filter(is_type<peg::argument>())
            | views::transform([](const auto& child) {
                return Argument {
                    .name = and_then(first_node<peg::argument_name>(*child), [](const auto* n) { return n->string(); }),
                    .value = ParseValue(*child).value_or(""),
                };
            })));
    }).value_or(vector<Argument>{});
}

}