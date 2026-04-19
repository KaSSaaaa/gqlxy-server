#pragma once

#include <gqlxy/parser/ast/fragments.h>
#include <gqlxy/resolvers.h>
#include <optional>
#include <string>
#include <vector>

namespace gqlxy::parser {
struct Directive;
struct SelectionSet;
struct Field;
}
namespace gqlxy::internal {

std::vector<parser::Field> FragmentFields(
    const parser::SelectionSet& selectionSet, const std::vector<parser::Directive>& fieldDirectives,
    const Directives& directives, const nlohmann::json& variables, const parser::Fragments& frags,
    const std::optional<std::string>& typeCondition, const std::optional<std::string>& concreteType);

std::vector<parser::Field> FlattenSelections(
    const parser::SelectionSet& ss, const parser::Fragments& frags, const Directives& directives,
    const nlohmann::json& variables, const std::optional<std::string>& concreteType = std::nullopt);

}
