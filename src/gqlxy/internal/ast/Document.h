#pragma once

#include "FragmentDefinition.h"
#include "OperationDefinition.h"

#include <string>
#include <unordered_map>
#include <vector>

namespace gqlxy::internal {

struct Document {
    std::vector<OperationDefinition> operations;
    std::unordered_map<std::string, FragmentDefinition> fragments;
};

}
