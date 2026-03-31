#pragma once

#include <functional>
#include <nlohmann/json.hpp>

namespace gqlxy {

class ScalarType {
public:
    ScalarType(const std::function<nlohmann::json()>& serialize)
        : _serialize(serialize) {

    }

    nlohmann::json Serialize() const {
        return _serialize();
    }

private:
    std::function<nlohmann::json()> _serialize;
};

}