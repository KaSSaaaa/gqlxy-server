#include "ParseDocument.h"

#include <ariane/internal/ast/Selection.h>
#include <ariane/internal/peg/is_type.h>
#include <ariane/internal/utils/ranges.h>
#include <graphqlservice/internal/Grammar.h>

#include "ParseFragmentDefinition.h"
#include "ParseOperationDefinition.h"

using namespace std;
using namespace graphql;

namespace ariane::graphql::internal {

Document ParseDocument(const string& query) {
    try {
        auto ast = peg::parseString(query);
        if (!ast.root)
            return {};

        return {
            .operations = to_vector(ast.root->children
                | views::filter(is_type<peg::operation_definition>())
                | views::transform([](const auto& child) {
                    return ParseOperationDefinition(*child);
                })),
            .fragments = to_unordered_map(ast.root->children
                | views::filter(is_type<peg::fragment_definition>())
                | views::transform([](const auto& child) {
                    auto frag = ParseFragmentDefinition(*child);
                    return make_pair(frag.name, frag);
                }))
        };
    } catch (const exception&) {
        return {};
    }
}

}