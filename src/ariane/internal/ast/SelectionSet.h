#pragma once

#include <vector>

namespace ariane::graphql::internal {
struct Selection;

struct SelectionSet {
    std::vector<Selection> selections;
};

}