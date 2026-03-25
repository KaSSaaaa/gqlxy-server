#include "ParseValue.h"

#include <gqlxy/internal/peg/first_node.h>
#include <graphqlservice/internal/Grammar.h>

using namespace std;
using namespace graphql;

namespace gqlxy::internal {

optional<string> ParseValue(const peg::ast_node& node) {
    if (find_node<peg::true_keyword>(node))
        return "true";
    if (find_node<peg::false_keyword>(node))
        return "false";
    if (find_node<peg::null_keyword>(node))
        return "null";
    if (auto v = find_node<peg::list_value>(node))
        return (*v)->string();
    if (auto v = find_node<peg::string_value>(node))
        return (*v)->string();
    if (auto v = find_node<peg::integer_value>(node))
        return (*v)->string();
    if (auto v = find_node<peg::float_value>(node))
        return (*v)->string();
    if (auto v = find_node<peg::enum_value>(node))
        return (*v)->string();
    if (auto v = find_node<peg::variable_value>(node))
        return (*v)->string();
    if (auto v = find_node<peg::object_value>(node))
        return (*v)->string();
    return nullopt;
}

}