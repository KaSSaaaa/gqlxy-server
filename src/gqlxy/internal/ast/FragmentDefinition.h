#pragma once

#include "SelectionSet.h"

#include <string>

namespace gqlxy::internal {

struct FragmentDefinition {
    std::string name;
    std::string typeCondition;
    SelectionSet selectionSet;
};

}
