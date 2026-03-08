#pragma once

#include <ariane/scalars.h>
#include <ariane/task.h>
#include <functional>
#include <future>
#include <list>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

namespace ariane::graphql {

struct ValueResolver;

struct ResolverArgs {
    nlohmann::json args = nlohmann::json::object();
};

using Resolver = std::unordered_map<std::string, ValueResolver>;
using FunctionResolver = std::function<ValueResolver(const ResolverArgs&)>;
using AsyncFunctionResolver = std::function<std::future<ValueResolver>(const ResolverArgs&)>;
using CoroutineResolver = std::function<Task<ValueResolver>(const ResolverArgs&)>;
using CallbackResolver = std::function<void(const ResolverArgs&, const std::function<void(const ValueResolver&)>&)>;
using OptionalFunctionResolver = std::function<std::optional<ValueResolver>()>;
using TypeResolver = std::function<std::optional<std::string>(const Resolver&)>;
using ScalarResolver = std::function<nlohmann::json(const nlohmann::json&)>;
using Scalars = std::unordered_map<std::string, ScalarResolver>;
using DirectiveResolver = std::function<std::optional<ValueResolver>(const ResolverArgs& args, const ValueResolver& value)>;
using Directives = std::unordered_map<std::string, DirectiveResolver>;

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
                                    TypeResolver,
                                    ScalarType,
                                    std::monostate> {
    using variant::variant;

    ValueResolver(std::nullopt_t) : variant(std::monostate{}) {}
    ValueResolver(std::nullptr_t) : variant(std::monostate{}) {}
    ValueResolver(const char* str) : variant(str) {}
    ValueResolver(std::initializer_list<std::pair<const std::string, ValueResolver>>&& init)
        : variant(Resolver(init.begin(), init.end())) {}
    ValueResolver(std::initializer_list<ValueResolver>&& list) : variant(std::vector(list)) {}
    ValueResolver(const std::list<ValueResolver>& list)
        : variant(std::vector(list.begin(), list.end())) {}

    template <typename T>
    ValueResolver(const std::optional<T>& opt) : variant(opt.has_value() ? variant(*opt) : variant(std::monostate{})) {}
};

}