#pragma once

#include <nlohmann/json_fwd.hpp>
#include <string>
#include <optional>

namespace gqlxy::internal {

struct Argument {
    std::string name;
    std::string value;

    bool IsVariable() const;
    std::string Reference() const;
    static std::string Reference(const std::string& name);
    std::optional<nlohmann::json> TryValue(const nlohmann::json& variables) const;
    nlohmann::json Value(const nlohmann::json& variables) const;
};

}
