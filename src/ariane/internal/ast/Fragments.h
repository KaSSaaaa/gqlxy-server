#pragma once

#include <unordered_map>

namespace ariane::graphql::internal {
struct FragmentDefinition;

using Fragments = std::unordered_map<std::string, FragmentDefinition>;

}