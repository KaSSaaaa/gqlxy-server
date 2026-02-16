#pragma once

#include <ariane/resolvers.h>

#include <string>

namespace ariane::graphql {

struct SchemaOptions {
    std::string typeDefs;
    Resolver resolvers;
};

class Schema {
   public:
    Schema(const SchemaOptions& options);
};

}