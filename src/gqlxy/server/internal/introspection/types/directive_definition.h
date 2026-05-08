#pragma once

#include <better-enums/enum.h>

#include <optional>
#include <string>
#include <vector>

#include "input_value_definition.h"

namespace gqlxy::internal {

BETTER_ENUM(DirectiveLocation,
            int,
            QUERY,
            MUTATION,
            SUBSCRIPTION,
            FIELD,
            FRAGMENT_DEFINITION,
            FRAGMENT_SPREAD,
            INLINE_FRAGMENT,
            SCHEMA,
            SCALAR,
            OBJECT,
            FIELD_DEFINITION,
            ARGUMENT_DEFINITION,
            INTERFACE,
            UNION,
            ENUM,
            ENUM_VALUE,
            INPUT_OBJECT,
            INPUT_FIELD_DEFINITION);

struct DirectiveDefinition {
    std::string name;
    std::optional<std::string> description;
    std::vector<DirectiveLocation> locations;
    std::vector<InputValueDefinition> args;
    bool isRepeatable = false;
};

}
