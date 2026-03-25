#pragma once

#include <gqlxy/resolvers.h>
#include <vector>

namespace gqlxy {
struct ValueResolver;
}

namespace gqlxy::internal {
struct Directive;

std::optional<ValueResolver> ApplyDirectives(const std::vector<Directive>& directives,
                                             const Directives& allDirectives,
                                             const nlohmann::json& variables,
                                             const ValueResolver& value);

}