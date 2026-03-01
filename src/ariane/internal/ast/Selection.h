#pragma once

#include "Argument.h"

#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace ariane::graphql::internal {

struct SelectionSet;

struct Field {
    std::optional<std::string> alias;
    std::string name;
    std::vector<Argument> arguments;
    std::shared_ptr<SelectionSet> selectionSet; //TODO Replace with optional<shared_ptr<SelectionSet>>
};

struct FragmentSpread {
    std::string name;
};

struct InlineFragment {
    std::optional<std::string> typeCondition;
    std::shared_ptr<SelectionSet> selectionSet; //TODO Replace with optional<shared_ptr<SelectionSet>>
};

using Selection = std::variant<Field, FragmentSpread, InlineFragment>;

}
