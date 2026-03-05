#pragma once

#include <ariane/resolvers.h>
#include <vector>

namespace ariane::graphql {
struct ValueResolver;
}

namespace ariane::graphql::internal {
struct Directive;

std::optional<ValueResolver> ApplyDirectives(const std::vector<Directive>& directives,
                                             const Directives& allDirectives,
                                             const nlohmann::json& variables,
                                             const ValueResolver& value);

}