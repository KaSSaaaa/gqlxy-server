#pragma once

#include <ariane/internal/ast/Document.h>

namespace ariane::graphql::internal {

Document ParseDocument(const std::string& query);

}