#pragma once

#include <ariane/internal/introspection/types/TypeRef.h>

#include <optional>
#include <string>

namespace ariane::graphql::internal {

struct VariableDefinition {
    std::string name;
    TypeRef type;
    std::optional<std::string> defaultValue;
};

}
