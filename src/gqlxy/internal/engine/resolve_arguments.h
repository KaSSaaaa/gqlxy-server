#pragma once

#include <gqlxy/parser/ast/argument.h>
#include <gqlxy/parser/ast/selection.h>
#include <gqlxy/resolvers.h>
#include <nlohmann/json.hpp>

namespace gqlxy::internal {
struct ResolveQueryArgs;

nlohmann::json ResolveArguments(
    const parser::Field& field,
    const ResolveQueryArgs& args,
    const std::string& typeName);

nlohmann::json ResolveArguments(const std::vector<parser::Argument>& arguments, const nlohmann::json& variables);

}