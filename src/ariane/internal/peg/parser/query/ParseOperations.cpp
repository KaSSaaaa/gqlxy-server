#include "ParseOperations.h"

#include "ParseDocument.h"
#include <ariane/internal/ast/Selection.h>

using namespace std;

namespace ariane::graphql::internal {

vector<OperationDefinition> ParseOperations(const string& query) {
    return ParseDocument(query).operations;
}

}

