#pragma once

#include <gqlxy/core/utils/ranges.h>
#include <gqlxy/server/definitions/directive_definition.h>
#include <gqlxy/server/definitions/type_definition.h>
#include <map>
#include <optional>
#include <ranges>
#include <string>
#include <vector>

namespace gqlxy {

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
