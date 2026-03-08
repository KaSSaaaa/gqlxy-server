#include "ResolveArguments.h"

#include <ariane/internal/ast/Selection.h>
#include <ariane/internal/introspection/types/SchemaDefinition.h>

#include <algorithm>

#include "resolve.h"

using namespace std;
using namespace nlohmann;

namespace ariane::graphql::internal {

//TODO clean this
vector<InputValueDefinition> FieldArgDefs(const string& typeName, const string& fieldName,
                                                        const SchemaDefinition& schema) {
    if (!schema.types.contains(typeName))
        return {};
    const auto& fields = schema.types.at(typeName).fields;
    auto it = ranges::find_if(fields, [&](const auto& f) { return f.name == fieldName; });
    return it != fields.end() ? it->args : vector<InputValueDefinition>();
}

json ResolveArguments(const Field& field,
                      const ResolveQueryArgs& args,
                      const string& typeName) {
    auto& [query, variables, schemaDefinition, resolvers, directives, scalars] = args;
    auto argDefs = FieldArgDefs(typeName, field.name, schemaDefinition);

    return accumulate(field.arguments.begin(), field.arguments.end(), json::object(), [&](auto obj, const auto& arg) {
        auto [name, rawValue] = arg;
        bool isVar = rawValue.starts_with("$");
        json value = isVar ? variables[rawValue.substr(1)] : json::parse(rawValue);

        auto it = ranges::find_if(argDefs, [&](const auto& d) { return d.name == name; });
        if (it != argDefs.end() && scalars.contains(it->type.typeName()))
            value = scalars.at(it->type.typeName())(value);

        obj[name] = value;
        return obj;
    });
}
json ResolveArguments(const vector<Argument>& arguments, const json& variables) {
    return accumulate(arguments.begin(), arguments.end(), json::object(), [&](auto obj, const auto& arg) {
        auto [name, rawValue] = arg;
        obj[name] = rawValue.starts_with("$") ? variables[rawValue.substr(1)] : json::parse(rawValue);
        return obj;
    });
}

}
