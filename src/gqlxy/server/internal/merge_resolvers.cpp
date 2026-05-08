#include "merge_resolvers.h"

#include <format>
#include <gqlxy/core/utils/expect.h>

using namespace std;
using namespace gqlxy::utils;

namespace gqlxy::internal {

void MergeResolvers(Resolver& left, const Resolver& right, const string& typeName) {
    for (const auto& [fieldName, fieldResolver] : right) {
        if (fieldName.starts_with("__")) {
            left.try_emplace(fieldName, fieldResolver);
            continue;
        }
        expect(!left.contains(fieldName), format("Conflicting resolver '{}.{}'", typeName, fieldName));
        left[fieldName] = fieldResolver;
    }
}

Resolver MergeResolvers(const Resolver& base, const Resolver& other) {
    Resolver result = base;
    for (const auto& [typeName, typeResolver] : other) {
        if (!result.contains(typeName))
            result[typeName] = typeResolver;
        else
            MergeResolvers(result[typeName].As<Resolver>(), typeResolver.As<Resolver>(), typeName);
    }
    return result;
}

}