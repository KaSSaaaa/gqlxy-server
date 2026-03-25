#pragma once

#include <gqlxy/internal/ast/Document.h>

namespace gqlxy::internal {

Document ParseDocument(const std::string& query);

}