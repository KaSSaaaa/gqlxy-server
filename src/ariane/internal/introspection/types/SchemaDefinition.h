#pragma once

#include <map>
#include <optional>
#include <string>
#include <vector>
#include <ranges>

#include "DirectiveDefinition.h"
#include "TypeDefinition.h"

namespace ariane::graphql::internal {

struct SchemaDefinition {
    std::map<std::string, TypeDefinition> types;
    std::vector<DirectiveDefinition> directives;
    std::optional<std::string> queryTypeName;
    std::optional<std::string> mutationTypeName;
    std::optional<std::string> subscriptionTypeName;

    auto InterfacesPerType() {
        using namespace std;
        return types | views::transform([](const auto& type) {
            return type.second.interfaces | views::transform([&](const auto& interface) { return make_pair(type.first, interface); });
        }) | views::join;
    }
};

}
