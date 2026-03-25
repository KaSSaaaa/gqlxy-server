#pragma once

#include <gqlxy/internal/introspection/types/TypeRef.h>

#include <optional>
#include <string>

namespace gqlxy::internal {

struct VariableDefinition {
    std::string name;
    TypeRef type;
    std::optional<std::string> defaultValue;
};

}
