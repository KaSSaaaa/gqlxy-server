#pragma once

#include <map>
#include <string>
#include <variant>
#include <functional>
#include <memory>

namespace ariane::graphql
{

class Selector;
 
using _BaseFieldValue = std::variant<
    std::string,
    int,
    double,
    bool
>;

class FieldResolver
{
public:
    FieldResolver() = default;
    
    template<typename T,
             typename = std::enable_if_t<
                 std::is_constructible_v<_BaseFieldValue, T> &&
                 !std::is_same_v<std::decay_t<T>, FieldResolver>
             >>
    FieldResolver(T&& val) : _scalar(std::forward<T>(val)) {}
    
    FieldResolver(Selector sel);
    
    FieldResolver(FieldResolver&&) = default;
    FieldResolver& operator=(FieldResolver&&) = default;
    
    FieldResolver(const FieldResolver& other) 
        : _scalar(other._scalar),
          _selector(other._selector ? std::make_unique<Selector>(*other._selector) : nullptr) {}
    
    FieldResolver& operator=(const FieldResolver& other) {
        if (this != &other) {
            _scalar = other._scalar;
            _selector = other._selector ? std::make_unique<Selector>(*other._selector) : nullptr;
        }
        return *this;
    }
    
    bool isSelector() const { return static_cast<bool>(_selector); }
    
    const _BaseFieldValue& scalar() const { return _scalar; }
    _BaseFieldValue& scalar() { return _scalar; }
    
    const Selector& selector() const;
    Selector& selector();

private:
    _BaseFieldValue _scalar;
    std::unique_ptr<Selector> _selector;
};

class Selector : public std::map<std::string, FieldResolver>
{
public:
    using std::map<std::string, FieldResolver>::map;
};

inline FieldResolver::FieldResolver(Selector sel) 
    : _selector(std::make_unique<Selector>(std::move(sel))) {}

inline const Selector& FieldResolver::selector() const { 
    return *_selector; 
}

inline Selector& FieldResolver::selector() { 
    return *_selector; 
}

using Resolver = std::map<std::string, Selector>;
}
