#pragma once

#include <map>
#include <string>
#include <variant>
#include <functional>
#include <memory>
#include <future>
#include <optional>

namespace ariane::graphql
{
    struct ValueResolver;

    using Resolver = std::unordered_map<std::string, ValueResolver>;
    using FunctionResolver = std::function<ValueResolver()>;
    using AsyncFunctionResolver = std::function<std::future<ValueResolver>()>;
    using OptionalFunctionResolver = std::function<std::optional<ValueResolver>()>;

    struct ValueResolver : std::variant<
        int,
        double,
        bool,
        std::string,
        Resolver,
        FunctionResolver,
        AsyncFunctionResolver
    >
    {
        using variant::variant;
    };
}
