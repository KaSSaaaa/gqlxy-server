#pragma once

#include <ariane/resolvers.h>

#include <memory>
#include <string>
#include <unordered_map>

namespace ariane::graphql {

namespace internal {
struct Document;
}

struct SchemaOptions {
    std::string typeDefs;
    Resolver resolvers;
    bool allowIntrospection = true;
};

struct ResolveResult {
    std::string data;
    std::string errors;
};

class Schema {
public:
    explicit Schema(const SchemaOptions& options);

    const internal::Document& GetDocument() const { return *_document; }
    const Resolver& GetResolvers() const { return _resolvers; }

    Task<ResolveResult> Resolve(const std::string& query,
                          const std::unordered_map<std::string, std::string>& variables = {});

private:
    std::shared_ptr<internal::Document> _document;
    Resolver _resolvers;

    void InjectIntrospectionResolvers();
};

}