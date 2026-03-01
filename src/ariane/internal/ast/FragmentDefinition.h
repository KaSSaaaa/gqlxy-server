#pragma once

#include "SelectionSet.h"

#include <string>

namespace ariane::graphql::internal {

struct FragmentDefinition {
    std::string name;
    std::string typeCondition;
    SelectionSet selectionSet;
};

}
