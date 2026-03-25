#include "ParseFragmentSpread.h"

#include <gqlxy/internal/peg/first_node.h>
#include <gqlxy/internal/utils/optional.h>
#include <graphqlservice/internal/Grammar.h>

#include "ParseDirectives.h"

using namespace std;
using namespace graphql;

namespace gqlxy::internal {

FragmentSpread ParseFragmentSpread(const peg::ast_node& node) {
    return FragmentSpread {
        .name = and_then(first_node<peg::fragment_name>(node), [](const auto* n) {
            return n->string();
        }),
        .directives = ParseDirectives(node),
    };
}

}