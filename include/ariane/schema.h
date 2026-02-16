#pragma once

#include <string>
#include <ariane/resolvers.h>

namespace ariane::graphql
{
    struct SchemaOptions
    {
        std::string typeDefs;
        Resolver resolvers;
    };

    class Schema
    {
    public:
        Schema(const SchemaOptions& options);
    };
}