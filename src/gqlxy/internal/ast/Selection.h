#pragma once

#include "SelectionSet.h"

#include <gqlxy/internal/ast/Argument.h>
#include <gqlxy/internal/ast/Directive.h>
#include <gqlxy/internal/ast/Fragments.h>
#include <gqlxy/resolvers.h>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace gqlxy::internal {

struct Field {
    std::optional<std::string> alias;
    std::string name;
    std::vector<Argument> arguments;
    std::vector<Directive> directives;
    std::optional<SelectionSet> selectionSet;
};

struct FragmentSpread {
    std::string name;
    std::vector<Directive> directives;
};

struct InlineFragment {
    std::optional<std::string> typeCondition;
    std::vector<Directive> directives;
    std::optional<SelectionSet> selectionSet;
};

struct Selection : std::variant<Field, FragmentSpread, InlineFragment> {};

std::vector<Field> FragmentFields(const SelectionSet& selectionSet,
                                  const std::vector<Directive>& fieldDirectives,
                                  const Directives& directives,
                                  const nlohmann::json& variables,
                                  const Fragments& frags,
                                  const std::optional<std::string>& typeCondition,
                                  const std::optional<std::string>& concreteType);

std::vector<Field> FlattenSelections(const SelectionSet& ss,
                                     const Fragments& frags,
                                     const Directives& directives,
                                     const nlohmann::json& variables,
                                     const std::optional<std::string>& concreteType = std::nullopt);

}
