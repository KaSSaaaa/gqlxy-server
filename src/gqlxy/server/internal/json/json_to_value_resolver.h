#pragma once
#include <gqlxy/server/resolvers.h>

namespace gqlxy::internal {

ValueResolver JsonToValueResolver(const nlohmann::json& j);

}