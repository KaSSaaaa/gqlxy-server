#include "ParseDocument.h"

#include <graphqlservice/internal/Grammar.h>

#include "ParseFragmentDefinition.h"
#include "ParseOperationDefinition.h"

using namespace std;
using namespace graphql;

namespace ariane::graphql::internal {

Document ParseDocument(const string& query) {
    try {
        auto ast = peg::parseString(query);
        if (!ast.root) return {};

        Document doc;
        for (const auto& child : ast.root->children) {
            if (!child) continue;
            if (child->is_type<peg::operation_definition>())
                doc.operations.push_back(ParseOperationDefinition(*child));
            else if (child->is_type<peg::fragment_definition>()) {
                auto frag = ParseFragmentDefinition(*child);
                doc.fragments[frag.name] = std::move(frag);
            }
        }
        return doc;
    } catch (const exception&) {
        return {};
    }
}

}