#pragma once
#include <ariane/resolvers.h>

namespace ariane::graphql::internal {

ValueResolver JsonToValueResolver(const nlohmann::json& j);

}