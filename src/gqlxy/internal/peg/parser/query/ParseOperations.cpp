#include "ParseOperations.h"

#include "ParseDocument.h"
#include <gqlxy/internal/ast/Selection.h>

using namespace std;

namespace gqlxy::internal {

vector<OperationDefinition> ParseOperations(const string& query) {
    return ParseDocument(query).operations;
}

}

