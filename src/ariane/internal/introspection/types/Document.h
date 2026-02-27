#pragma once

#include <map>
#include <optional>
#include <string>
#include <vector>

#include "DirectiveDefinition.h"
#include "TypeDefinition.h"

namespace ariane::graphql::internal {

struct Document {
    std::map<std::string, TypeDefinition> types;
    std::vector<DirectiveDefinition> directives;
    std::optional<std::string> queryTypeName;
    std::optional<std::string> mutationTypeName;
    std::optional<std::string> subscriptionTypeName;
};

}
