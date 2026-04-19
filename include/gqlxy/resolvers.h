#pragma once

#include "subscription.h"
#include <functional>
#include <future>
#include <gqlxy/resolvers/coroutine_resolver.h>
#include <gqlxy/scalars.h>
#include <gqlxy/utils.h>
#include <nlohmann/json.hpp>
#include <optional>
#include <ranges>
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
    ValueResolver() = default;
    ValueResolver(const ValueResolver&) = default;
    ValueResolver(ValueResolver&&) = default;
    ValueResolver& operator=(const ValueResolver&) = default;
    ValueResolver& operator=(ValueResolver&&) = default;

    ValueResolver(int v) : variant(v) {}
    ValueResolver(uint64_t v) : variant(v) {}
    ValueResolver(double v) : variant(v) {}
    ValueResolver(float v) : variant(v) {}
    template <typename T>
        requires std::same_as<std::remove_cvref_t<T>, bool>
    ValueResolver(T v) : variant(static_cast<bool>(v)) {}
    ValueResolver(std::string v) : variant(std::move(v)) {}
    ValueResolver(Resolver v) : variant(std::move(v)) {}
    ValueResolver(std::vector<ValueResolver> v) : variant(std::move(v)) {}
    ValueResolver(FunctionResolver v) : variant(std::move(v)) {}
    ValueResolver(AsyncFunctionResolver v) : variant(std::move(v)) {}
    ValueResolver(CoroutineResolver v) : variant(std::move(v)) {}
    ValueResolver(CallbackResolver v) : variant(std::move(v)) {}
    ValueResolver(TypeResolver v) : variant(std::move(v)) {}
    ValueResolver(SubscriptionResolver v) : variant(std::move(v)) {}
    ValueResolver(ScalarType v) : variant(std::move(v)) {}
    ValueResolver(std::monostate v) : variant(v) {}
    ValueResolver(std::nullopt_t) : variant(std::monostate {}) {}
    ValueResolver(std::nullptr_t) : variant(std::monostate {}) {}
    ValueResolver(const char* str) : variant(std::string(str)) {}

    ValueResolver(std::initializer_list<std::pair<const std::string, ValueResolver>>&& init)
        : variant(Resolver(init.begin(), init.end())) {}
    ValueResolver(std::initializer_list<ValueResolver>&& list) : variant(std::vector(list)) {}

    template <std::ranges::input_range R>
        requires std::convertible_to<std::ranges::range_value_t<R>, ValueResolver> &&
                 (!std::same_as<std::remove_cvref_t<R>, std::string>) &&
                 (!std::same_as<std::remove_cvref_t<R>, Resolver>)
    ValueResolver(R&& range) : variant(std::vector<ValueResolver>(std::ranges::begin(range), std::ranges::end(range))) {}

    template <typename T>
        requires better_enum_value<T> || better_enum_entry<T>
    ValueResolver(T e) : variant([&]() -> std::string {
        if constexpr (better_enum_value<T>) return std::string(e._to_string());
        else return std::string((+e)._to_string());
    }()) {}

    template <typename F>
        requires(!better_enum_value<std::remove_cvref_t<F>>) && (!better_enum_entry<std::remove_cvref_t<F>>) &&
                (!std::same_as<std::remove_cvref_t<F>, bool>) && (!is_optional<std::remove_cvref_t<F>>) &&
                (!std::same_as<std::remove_cvref_t<F>, ValueResolver>) &&
                (!std::ranges::input_range<std::remove_cvref_t<F>>) && requires { variant(std::declval<F>()); }
    ValueResolver(F&& f) : variant(std::forward<F>(f)) {}

    template <typename T>
        requires is_optional<std::optional<T>>
    ValueResolver(const std::optional<T>& opt)
        : ValueResolver(opt.has_value() ? ValueResolver(opt.value()) : ValueResolver(std::monostate {})) {}

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