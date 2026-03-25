#pragma once

#include <gqlxy/resolvers.h>
#include <gqlxy/internal/introspection/types/SchemaDefinition.h>

#include <memory>
#include <string>

namespace gqlxy::internal {

struct FederationOptions {
    std::string typeDefs;
};

void InjectFederation(
    const std::shared_ptr<SchemaDefinition>& schema,
    Resolver& resolvers,
    const FederationOptions& options
);

}
