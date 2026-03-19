#pragma once

#include <nlohmann/json.hpp>
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
    std::optional<nlohmann::json> data;
    std::optional<FieldErrors> errors;
};

nlohmann::json Serialize(const ResolveResult& result);

}
