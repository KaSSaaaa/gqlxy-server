#pragma once

#include <ariane/resolvers.h>

namespace ariane::graphql::internal {

void MergeResolvers(Resolver& left, const Resolver& right, const std::string& typeName = "");
Resolver MergeResolvers(const Resolver& left, const Resolver& right);

}
