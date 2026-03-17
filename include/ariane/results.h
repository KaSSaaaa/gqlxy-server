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

inline nlohmann::json Serialize(const ResolveResult& result) {
    nlohmann::json r;
    if (result.data.has_value()) r["data"] = result.data.value();
    if (result.errors.has_value()) {
        auto err = nlohmann::json::array();
        for (const auto& e : result.errors.value()) {
            err.push_back({
                {"message", e.message}
            });
        }
        r["errors"] = err;
    }
    return r;
}

}
