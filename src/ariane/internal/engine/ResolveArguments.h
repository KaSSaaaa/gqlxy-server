#pragma once
#include <nlohmann/json.hpp>
#include <vector>

namespace ariane::graphql::internal {
struct Argument;

nlohmann::json ResolveArguments(const std::vector<Argument>& args, const nlohmann::json& variables);

}