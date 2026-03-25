#pragma once

#include <unordered_map>
#include <string>

namespace gqlxy::internal {
struct FragmentDefinition;

using Fragments = std::unordered_map<std::string, FragmentDefinition>;

}