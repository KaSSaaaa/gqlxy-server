#pragma once

#include "SelectionSet.h"
#include "VariableDefinition.h"

#include <better-enums/enum.h>

#include <optional>
#include <string>
#include <vector>

namespace ariane::graphql::internal {

BETTER_ENUM(OperationType, int, QUERY, MUTATION, SUBSCRIPTION);

struct OperationDefinition {
    OperationType type = OperationType::QUERY;
    std::optional<std::string> name;
    std::vector<VariableDefinition> variableDefinitions;
    SelectionSet selectionSet;
};

}
