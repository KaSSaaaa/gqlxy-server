#pragma once

#include <optional>
#include <string>

namespace ariane::graphql::internal {

struct EnumValueDefinition {
    std::string name;
    std::optional<std::string> description;
    bool isDeprecated = false;
    std::optional<std::string> deprecationReason;
};

}
