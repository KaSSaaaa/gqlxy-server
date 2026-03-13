#pragma once

#include <ariane/resolvers.h>
#include <ariane/internal/introspection/types/SchemaDefinition.h>

#include <memory>
#include <string>

namespace ariane::graphql::internal {

struct FederationOptions {
    std::string typeDefs;
};

void InjectFederation(
    const std::shared_ptr<SchemaDefinition>& schema,
    Resolver& resolvers,
    const FederationOptions& options
);

}
