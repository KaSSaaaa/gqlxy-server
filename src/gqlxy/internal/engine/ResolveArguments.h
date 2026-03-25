#pragma once

#include <gqlxy/resolvers.h>
#include <nlohmann/json.hpp>

namespace gqlxy::internal {
struct Field;
struct ResolveQueryArgs;
struct Argument;

nlohmann::json ResolveArguments(
    const Field& field,
    const ResolveQueryArgs& args,
    const std::string& typeName);

nlohmann::json ResolveArguments(const std::vector<Argument>& arguments, const nlohmann::json& variables);

}