#pragma once

#include "SelectionSet.h"

#include <ariane/internal/ast/Argument.h>
#include <ariane/internal/ast/Directive.h>
#include <ariane/internal/ast/Fragments.h>
#include <ariane/resolvers.h>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace ariane::graphql::internal {

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

struct Selection : std::variant<Field, FragmentSpread, InlineFragment> {

};

std::vector<Field> FragmentFields(const SelectionSet& selectionSet,
                                  const std::vector<Directive>& fieldDirectives,
                                  const Directives& directives,
                                  const nlohmann::json& variables,
                                  const Fragments& frags,
                                  const std::optional<std::string>& typeCondition,
                                  const std::optional<std::string>& concreteType);

std::vector<Field> FlattenSelections(const SelectionSet& ss, const Fragments& frags,
                                     const Directives& directives, const nlohmann::json& variables,
                                     const std::optional<std::string>& concreteType = std::nullopt);

}
