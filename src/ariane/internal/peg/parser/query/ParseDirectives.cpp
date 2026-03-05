#include "ParseDirectives.h"

#include <ariane/internal/peg/first_node.h>
#include <ariane/internal/peg/is_type.h>
#include <ariane/internal/utils/optional.h>
#include <ariane/internal/utils/ranges.h>
#include <graphqlservice/internal/Grammar.h>
#include "ParseArguments.h"

using namespace std;
using namespace graphql;

namespace ariane::graphql::internal {

vector<Directive> ParseDirectives(const peg::ast_node& node) {
    return and_then(first_node<peg::directives>(node), [](const auto* directives) {
        return make_optional(to_vector(directives->children
            | views::filter(is_type<peg::directive>())
            | views::transform([](const auto& child) {
                return Directive{
                    .name = find_node<peg::directive_name>(*child).value()->string(),
                    .args = ParseArguments(*child),
                };
            })));
    }).value_or(vector<Directive>{});
}

}
