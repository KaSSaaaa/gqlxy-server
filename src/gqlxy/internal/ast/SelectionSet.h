#pragma once

#include <vector>

namespace gqlxy::internal {
struct Selection;

struct SelectionSet {
    std::vector<Selection> selections;
};

}