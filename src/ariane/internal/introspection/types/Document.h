#pragma once

#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "DirectiveDefinition.h"
#include "TypeDefinition.h"

namespace ariane::graphql::internal {

struct Document {
    std::unordered_map<std::string, TypeDefinition> types;
    std::vector<std::string> typeOrder;
    std::vector<DirectiveDefinition> directives;
    std::optional<std::string> queryTypeName;
    std::optional<std::string> mutationTypeName;
    std::optional<std::string> subscriptionTypeName;
};

}
