#include "ParseOperationDefinition.h"

#include <ariane/internal/peg/first_node.h>
#include <ariane/internal/peg/transform_children.h>
#include <ariane/internal/utils/optional.h>
#include <graphqlservice/internal/Grammar.h>

#include "ParseSelectionSet.h"
#include "ParseVariableDefinition.h"

using namespace std;
using namespace graphql;

namespace ariane::graphql::internal {

OperationDefinition ParseOperationDefinition(const peg::ast_node& node) {
    return OperationDefinition {
        .type = OperationType::_from_string_nocase(and_then(first_node<peg::operation_type>(node), [](const auto* n) {
            return make_optional(n->string());
        }).value_or("query").c_str()),
        .name = and_then(first_node<peg::operation_name>(node), [](const auto* n) {
            return make_optional(n->string());
        }),
        .variableDefinitions = transform_children<peg::variable, VariableDefinition>(node, [](const auto& child) {
            return ParseVariableDefinition(child);
        }),
        .selectionSet = and_then(first_node<peg::selection_set>(node), [](const auto* selectionSetNode) {
            return make_optional(ParseSelectionSet(*selectionSetNode));
        }).value_or(SelectionSet{}),
    };
}

}