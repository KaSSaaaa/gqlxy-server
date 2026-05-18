#pragma once

#include <gqlxy/server/resolvers.h>

namespace gqlxy::internal {

void MergeResolvers(Resolver& left, const Resolver& right, const std::string& typeName = "");
Resolver MergeResolvers(const Resolver& left, const Resolver& right);

}
