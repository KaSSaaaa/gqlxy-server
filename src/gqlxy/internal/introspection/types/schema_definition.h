#pragma once

#include "directive_definition.h"
#include "type_definition.h"
#include <gqlxy/utils/ranges.h>
#include <map>
#include <optional>
#include <ranges>
#include <string>
#include <vector>

namespace gqlxy::internal {

struct SchemaDefinition {
    std::map<std::string, TypeDefinition> types;
    std::vector<DirectiveDefinition> directives;
    std::optional<std::string> queryTypeName;
    std::optional<std::string> mutationTypeName;
    std::optional<std::string> subscriptionTypeName;

    auto InterfacesPerType() {
        using namespace std;
        using namespace gqlxy::utils;
        return flat_map(types, [](const auto& type) {
            return type.second.interfaces | views::transform([&](const auto& interface) { return make_pair(type.first, interface); });
        });
    }
};

}
