#pragma once

#include <nlohmann/json.hpp>

namespace gqlxy {

struct ResolverArgsParams {
    nlohmann::json args = nlohmann::json::object();
    std::any context = {};
};

class ResolverArgs {
public:
    explicit ResolverArgs(const ResolverArgsParams& params = {})
        : _args(params.args),
          _context(params.context) {

    }

    const nlohmann::json& Args() const { return _args; }

    template<typename T>
    T& Context() { return std::any_cast<T&>(_context); }

    template<typename T>
    const T& Context() const { return std::any_cast<const T&>(_context); }

private:
    nlohmann::json _args;
    std::any _context;
};

}