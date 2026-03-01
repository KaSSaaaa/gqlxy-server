#pragma once

#include "Selection.h"
#include <vector>

namespace ariane::graphql::internal {

struct SelectionSet {
    std::vector<Selection> selections;
};

}