#pragma once

#include "Argument.h"

#include <string>
#include <vector>

namespace gqlxy::internal {

struct Directive {
    std::string name;
    std::vector<Argument> args;
};

}
