#pragma once

#include <gqlxy/server/definitions/schema_definition.h>
#include <gqlxy/server/resolvers.h>
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
