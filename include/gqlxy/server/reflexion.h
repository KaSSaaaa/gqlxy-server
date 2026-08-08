#pragma once

#include <boost/pfr.hpp>
#include <gqlxy/server/resolvers.h>
#include <gqlxy/server/utils.h>
#include <string>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <variant>
#include <vector>

#define GQL_TYPE(TypeName) static constexpr const char* GQLName = #TypeName;

namespace gqlxy {

// Reserved, non-selectable key stamped into every ToResolver<T>() output so that
// interface/union fields can be resolved to their concrete GraphQL type automatically.
inline constexpr const char* GQLTypeNameKey = "__gqlxyTypeName";

template<typename T>
concept GQLReflectable = requires {
    { T::GQLName } -> std::convertible_to<const char*>;
};

struct GQLFieldOptions {
    std::optional<std::string> name;
    std::optional<std::string> type;
};

template<typename T>
struct GQLFieldOverrides {
    static const std::unordered_map<std::string, GQLFieldOptions>& Overrides() {
        static const std::unordered_map<std::string, GQLFieldOptions> empty;
        return empty;
    }
};

template<typename T>
struct GQLImplements {
    using Interfaces = std::tuple<>;
};

namespace internal {

template<typename>
inline constexpr bool is_vector = false;
template<typename T>
inline constexpr bool is_vector<std::vector<T>> = true;

template<typename>
inline constexpr bool is_variant = false;
template<typename... Ts>
inline constexpr bool is_variant<std::variant<Ts...>> = true;

template<typename T>
struct GQLScalarName {
    static_assert(sizeof(T) == 0, "no default GraphQL scalar mapping for this type; add a GQLFieldOverrides<T> entry");
};
template<>
struct GQLScalarName<std::string> {
    static constexpr const char* value = "String";
};
template<>
struct GQLScalarName<int> {
    static constexpr const char* value = "Int";
};
template<>
struct GQLScalarName<uint64_t> {
    static constexpr const char* value = "Int";
};
template<>
struct GQLScalarName<double> {
    static constexpr const char* value = "Float";
};
template<>
struct GQLScalarName<float> {
    static constexpr const char* value = "Float";
};
template<>
struct GQLScalarName<bool> {
    static constexpr const char* value = "Boolean";
};

template<typename F>
std::string GQLTypeString();

template<typename D>
std::string GQLCoreTypeName() {
    if constexpr (is_vector<D>) {
        return "[" + GQLTypeString<typename D::value_type>() + "]";
    } else if constexpr (GQLReflectable<D>) {
        return std::string(D::GQLName);
    } else if constexpr (is_variant<D>) {
        static_assert(
            sizeof(D) == 0, "unions can't be used as nested struct fields; register them via SchemaBuilder::AddUnion "
                            "and declare the field's SDL type manually");
    } else {
        return std::string(GQLScalarName<D>::value);
    }
}

template<typename F>
std::string GQLTypeString() {
    using D = std::remove_cvref_t<F>;
    if constexpr (is_optional<D>) {
        return GQLCoreTypeName<typename D::value_type>();
    } else {
        return GQLCoreTypeName<D>() + "!";
    }
}

inline std::string ApplyIdConvention(const std::string& fieldName, const std::string& sdlType) {
    if (fieldName != "id") return sdlType;
    if (sdlType == "String!") return "ID!";
    if (sdlType == "String") return "ID";
    return sdlType;
}

template<typename T>
std::optional<GQLFieldOptions> FindOverride(const std::string& fieldName) {
    const auto& overrides = GQLFieldOverrides<T>::Overrides();
    auto it = overrides.find(fieldName);
    if (it == overrides.end()) return std::nullopt;
    return it->second;
}

template<typename T, std::size_t I>
std::string GQLFieldSdlType(const std::string& fieldName) {
    const std::string defaultType = ApplyIdConvention(fieldName, GQLTypeString<boost::pfr::tuple_element_t<I, T>>());
    const auto override_ = FindOverride<T>(fieldName);
    return (override_ && override_->type) ? *override_->type : defaultType;
}

template<typename T, std::size_t I>
std::string GQLFieldSdlName(const std::string& fieldName) {
    const auto override_ = FindOverride<T>(fieldName);
    return (override_ && override_->name) ? *override_->name : fieldName;
}

template<typename T, std::size_t... Is>
std::string SdlFieldsFor(std::index_sequence<Is...>) {
    constexpr auto names = boost::pfr::names_as_array<T>();
    std::string body;
    ((body += "  " + GQLFieldSdlName<T, Is>(std::string(names[Is])) + ": " +
              GQLFieldSdlType<T, Is>(std::string(names[Is])) + "\n"),
     ...);
    return body;
}

template<typename... Is>
std::string ImplementsClauseFor(const std::tuple<Is...>&) {
    if constexpr (sizeof...(Is) == 0) return "";
    else
        return " implements " + ([]() {
                   std::string joined;
                   ((joined += (joined.empty() ? "" : " & ") + std::string(Is::GQLName)), ...);
                   return joined;
               })();
}

}

// Emits `interface Name { ... }` for T; use for types only ever referenced via GQLImplements.
template<typename T>
    requires GQLReflectable<T>
std::string SdlInterfaceDef() {
    const std::string fields = internal::SdlFieldsFor<T>(std::make_index_sequence<boost::pfr::tuple_size_v<T>>());
    return "interface " + std::string(T::GQLName) + " {\n" + fields + "}\n";
}

// Emits `type Name [implements I1 & I2] { ... }` for T, derived from T::GQLName, its reflected
// fields, and GQLImplements<T>::Interfaces.
template<typename T>
    requires GQLReflectable<T>
std::string SdlTypeDef() {
    const std::string fields = internal::SdlFieldsFor<T>(std::make_index_sequence<boost::pfr::tuple_size_v<T>>());
    const std::string implementsClause = internal::ImplementsClauseFor(typename GQLImplements<T>::Interfaces {});
    return "type " + std::string(T::GQLName) + implementsClause + " {\n" + fields + "}\n";
}

template<typename F>
ValueResolver GQLValueOf(const F& value);

template<typename T>
    requires GQLReflectable<T>
Resolver ToResolver(const T& obj) {
    Resolver resolver;
    boost::pfr::for_each_field(obj, [&](const auto& field, std::size_t index) {
        constexpr auto names = boost::pfr::names_as_array<T>();
        resolver[std::string(names[index])] = GQLValueOf(field);
    });
    resolver[GQLTypeNameKey] = std::string(T::GQLName);
    return resolver;
}

template<typename F>
ValueResolver GQLValueOf(const F& value) {
    using D = std::remove_cvref_t<F>;
    if constexpr (is_optional<D>) {
        return value.has_value() ? GQLValueOf(*value) : ValueResolver(std::monostate {});
    } else if constexpr (internal::is_vector<D>) {
        std::vector<ValueResolver> result;
        result.reserve(value.size());
        for (const auto& element : value)
            result.push_back(GQLValueOf(element));
        return result;
    } else if constexpr (internal::is_variant<D>) {
        return std::visit([](const auto& alt) -> ValueResolver { return GQLValueOf(alt); }, value);
    } else if constexpr (GQLReflectable<D>) {
        return ToResolver(value);
    } else {
        return ValueResolver(value);
    }
}

}
