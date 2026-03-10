#include "ParseVariableDefinition.h"

#include <ariane/internal/ast/Argument.h>
#include <ariane/internal/peg/first_node.h>
#include <ariane/internal/peg/parser/schema_parser.h>
#include <ariane/internal/utils/optional.h>
#include <graphqlservice/internal/Grammar.h>

using namespace std;
using namespace graphql;

namespace ariane::graphql::internal {

VariableDefinition ParseVariableDefinition(const peg::ast_node& node) {
    return VariableDefinition {
        .name = and_then(first_node<peg::variable_name>(node), [](const auto* n) {
            return Argument::Reference(n->string());
        }),
        .type = or_else(and_then(first_node<peg::nonnull_type>(node), [](const auto* n) {
            return make_optional(ParseTypeRef(*n));
        }), [&node]() {
            return or_else(and_then(first_node<peg::list_type>(node), [](const auto* n) {
                return make_optional(ParseTypeRef(*n));
            }), [&node]() {
                return and_then(first_node<peg::named_type>(node), [](const auto* n) {
                    return make_optional(ParseTypeRef(*n));
                });
            });
        }).value_or(TypeRef::Named("Unknown")),
        .defaultValue = and_then(first_node<peg::default_value>(node), [](const auto* dv) {
            if (!dv->children.empty() && dv->children[0] && dv->children[0]->has_content())
                return make_optional(dv->children[0]->string());
            return make_optional(string{});
        }),
    };
}

}