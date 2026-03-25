#pragma once
#include <gqlxy/resolvers.h>

namespace gqlxy::internal {

ValueResolver JsonToValueResolver(const nlohmann::json& j);

}