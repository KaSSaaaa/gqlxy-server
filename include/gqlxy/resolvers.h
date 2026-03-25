#pragma once

#include "subscription.h"
#include <gqlxy/scalars.h>
#include <gqlxy/resolvers/CoroutineResolver.h>
#include <functional>
#include <future>
#include <list>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

namespace gqlxy {
class ResolverArgs;

struct ValueResolver;

using Resolver = std::unordered_map<std::string, ValueResolver>;
using FunctionResolver = std::function<ValueResolver(const ResolverArgs&)>;
using AsyncFunctionResolver = std::function<std::future<ValueResolver>(const ResolverArgs&)>;
using CallbackResolver = std::function<void(const ResolverArgs&, const std::function<void(const ValueResolver&)>&)>;
using OptionalFunctionResolver = std::function<std::optional<ValueResolver>()>;
using TypeResolver = std::function<std::optional<std::string>(const Resolver&)>;
using SubscriptionResolver = std::function<SubscriptionEventStream(const ResolverArgs&)>;
using EntityResolver = std::function<ValueResolver(const ResolverArgs&)>;
using EntityResolvers = std::unordered_map<std::string, EntityResolver>;
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
                                    SubscriptionResolver,
                                    ScalarType,
                                    std::monostate> {
    using variant::variant;

    ValueResolver(std::nullopt_t) : variant(std::monostate{}) {}
    ValueResolver(std::nullptr_t) : variant(std::monostate{}) {}
    ValueResolver(const char* str) : variant(str) {}
    ValueResolver(std::initializer_list<std::pair<const std::string, ValueResolver>>&& init)
        : variant(Resolver(init.begin(), init.end())) {}
    ValueResolver(std::initializer_list<ValueResolver>&& list) : variant(std::vector(list)) {}
    ValueResolver(const std::list<ValueResolver>& list) : variant(std::vector(list.begin(), list.end())) {}

    template <typename T>
    ValueResolver(const std::optional<T>& opt) : variant(opt.has_value() ? variant(opt.value()) : variant(std::monostate{})) {}

    template <typename T>
    bool Is() const {
        return std::holds_alternative<T>(*this);
    }

    template <typename T>
    const T& As() const {
        return std::get<T>(*this);
    }

    template <typename T>
    T& As() {
        return std::get<T>(*this);
    }

    template <typename T>
    std::optional<T> AsIf() const {
        return Is<T>() ? std::make_optional(std::get<T>(*this)) : std::nullopt;
    }

    bool IsNull() const {
        return Is<std::monostate>();
    }

};

}