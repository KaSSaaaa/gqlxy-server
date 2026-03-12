#include "MergeResolvers.h"

#include <ariane/internal/utils/expect.h>

using namespace std;

namespace ariane::graphql::internal {

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

}