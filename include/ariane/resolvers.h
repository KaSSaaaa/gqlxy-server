#pragma once

#include <functional>
#include <future>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <variant>

namespace ariane::graphql {

struct ValueResolver;

using Resolver = std::unordered_map<std::string, ValueResolver>;
using FunctionResolver = std::function<ValueResolver()>;
using AsyncFunctionResolver = std::function<std::future<ValueResolver>()>;
using OptionalFunctionResolver = std::function<std::optional<ValueResolver>()>;

struct ValueResolver
    : std::variant<int, double, bool, std::string, Resolver, FunctionResolver, AsyncFunctionResolver, std::monostate> {
    using variant::variant;

    ValueResolver(std::nullopt_t) : variant(std::monostate{}) {}

    ValueResolver(const std::optional<int>& opt)
        : variant(opt.has_value() ? variant(*opt) : variant(std::monostate{})) {}

    ValueResolver(const std::optional<double>& opt)
        : variant(opt.has_value() ? variant(*opt) : variant(std::monostate{})) {}

    ValueResolver(const std::optional<bool>& opt)
        : variant(opt.has_value() ? variant(*opt) : variant(std::monostate{})) {}

    ValueResolver(const std::optional<std::string>& opt)
        : variant(opt.has_value() ? variant(*opt) : variant(std::monostate{})) {}

    ValueResolver(const std::optional<Resolver>& opt)
        : variant(opt.has_value() ? variant(*opt) : variant(std::monostate{})) {}
};

}