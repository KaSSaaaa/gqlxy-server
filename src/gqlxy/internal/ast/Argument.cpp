#include "Argument.h"
#include <nlohmann/json.hpp>

using namespace std;
using namespace gqlxy::internal;
using namespace gqlxy;
using namespace nlohmann;

bool Argument::IsVariable() const {
    return value.starts_with("$");
}

string Argument::Reference() const {
    return Reference(value);
}

string Argument::Reference(const string& name) {
    return name.substr(1);
}

optional<json> Argument::TryValue(const json& variables) const {
    if (!IsVariable()) return json::parse(value);
    return variables.contains(Reference()) ? make_optional(variables[Reference()]) : nullopt;
}

json Argument::Value(const json& variables) const {
    return TryValue(variables).value_or(nullptr);
}