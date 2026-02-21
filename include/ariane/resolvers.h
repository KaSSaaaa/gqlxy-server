#pragma once

#include <ariane/task.h>
#include <functional>
#include <future>
#include <list>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

namespace ariane::graphql {

struct ValueResolver;

using Resolver = std::unordered_map<std::string, ValueResolver>;
using FunctionResolver = std::function<ValueResolver()>;
using AsyncFunctionResolver = std::function<std::future<ValueResolver>()>;
using CoroutineResolver = std::function<Task<ValueResolver>()>;
using CallbackResolver = std::function<void(const std::function<void(const ValueResolver&)>&)>;
using OptionalFunctionResolver = std::function<std::optional<ValueResolver>()>;

struct ValueResolver : std::variant<int,
                                    uint64_t,
                                    double,
                                    float,
                                    bool,
                                    std::string,
                                    Resolver,
                                    std::vector<ValueResolver>,
                                    FunctionResolver,
                                    AsyncFunctionResolver,
                                    CoroutineResolver,
                                    CallbackResolver,
                                    std::monostate> {
    using variant::variant;

    ValueResolver(std::nullopt_t) : variant(std::monostate{}) {}
    ValueResolver(std::nullptr_t) : variant(std::monostate{}) {}
    ValueResolver(const char* str) : variant(std::string(str)) {}
    ValueResolver(std::initializer_list<std::pair<const std::string, ValueResolver>>&& init)
        : variant(Resolver(init.begin(), init.end())) {}
    ValueResolver(std::initializer_list<ValueResolver>&& list) : variant(std::vector<ValueResolver>(list)) {}
    ValueResolver(const std::list<ValueResolver>& list)
        : variant(std::vector<ValueResolver>(list.begin(), list.end())) {}

    template <typename T>
    ValueResolver(const std::optional<T>& opt) : variant(opt.has_value() ? variant(*opt) : variant(std::monostate{})) {}
};

}