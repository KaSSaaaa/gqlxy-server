#include "ParseArguments.h"

#include <ariane/internal/peg/first_node.h>
#include <ariane/internal/peg/transform_children.h>
#include <ariane/internal/utils/optional.h>
#include <graphqlservice/internal/Grammar.h>

#include "ParseArgumentValue.h"

using namespace std;
using namespace graphql;

namespace ariane::graphql::internal {

vector<Argument> ParseArguments(const peg::ast_node& node) {
    return and_then(first_node<peg::arguments>(node), [](const auto* argsNode) {
        return make_optional(transform_children<peg::argument, Argument>(*argsNode, [](const auto& child) {
            return Argument {
                .name = and_then(first_node<peg::argument_name>(child), [](const auto* n) { return n->string(); }),
                .value = ParseArgumentValue(child),
            };
        }));
    }).value_or(vector<Argument>{});
}

}