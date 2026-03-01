#pragma once

#include <ariane/internal/ast/OperationDefinition.h>
#include <string>

namespace ariane::graphql::internal {

std::vector<OperationDefinition> ParseOperations(const std::string& query);

}
