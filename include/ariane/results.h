#pragma once

#include <optional>
#include <string>
#include <vector>

namespace ariane::graphql {

struct ErrorLocation {
    int line;
    int column;
};

struct FieldError {
    std::string message;
    std::vector<std::string> path;
    std::vector<ErrorLocation> locations;
};

using FieldErrors = std::vector<FieldError>;

struct ResolveResult {
    std::optional<std::string> data;
    std::optional<FieldErrors> errors;
};

}
