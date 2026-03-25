#pragma once

#include <gqlxy/internal/ast/OperationDefinition.h>
#include <string>

namespace gqlxy::internal {

std::vector<OperationDefinition> ParseOperations(const std::string& query);

}
