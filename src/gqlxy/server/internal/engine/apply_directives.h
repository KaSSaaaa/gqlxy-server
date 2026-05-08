#pragma once

#include <gqlxy/core/parser/ast/directive.h>
#include <gqlxy/server/resolvers.h>
#include <vector>

namespace gqlxy {
struct ValueResolver;
}

namespace gqlxy::internal {

std::optional<ValueResolver> ApplyDirectives(const std::vector<parser::Directive>& directives,
                                             const Directives& allDirectives,
                                             const nlohmann::json& variables,
                                             const ValueResolver& value);

}