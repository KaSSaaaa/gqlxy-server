#pragma once

#include <string>

namespace ariane::graphql::internal {

struct Argument {
    std::string name;
    std::string value;
};

}
