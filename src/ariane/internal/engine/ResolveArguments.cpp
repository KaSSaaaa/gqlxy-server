#include "ResolveArguments.h"

#include <ariane/internal/ast/Selection.h>
#include <ariane/internal/introspection/types/SchemaDefinition.h>

#include <algorithm>

#include "ariane/internal/utils/ranges.h"
#include "resolve.h"

using namespace std;
using namespace nlohmann;

namespace ariane::graphql::internal {

vector<InputValueDefinition> FieldArgDefs(const string& typeName, const string& fieldName,
                                                        const SchemaDefinition& schema) {
    if (!schema.types.contains(typeName))
        return {};

    return concat(schema.types.at(typeName).fields
        | views::filter([&](const auto& f) { return f.name == fieldName; })
        | views::transform([](const auto& f) { return f.args; }) | views::join);
}

json ResolveArguments(const Field& field,
                      const ResolveQueryArgs& args,
                      const string& typeName) {
    auto& [query, variables, schemaDefinition, resolvers, directives, scalars, operationName, context] = args;
    auto argDefs = FieldArgDefs(typeName, field.name, schemaDefinition);

    return accumulate(field.arguments.begin(), field.arguments.end(), json::object(), [&](auto obj, const auto& arg) {
        auto value = arg.Value(variables);

        auto it = ranges::find_if(argDefs, [&](const auto& d) { return d.name == arg.name; });
        if (it != argDefs.end() && scalars.contains(it->type.TypeName()))
            value = scalars.at(it->type.TypeName())(value);

        obj[arg.name] = value;
        return obj;
    });
}
json ResolveArguments(const vector<Argument>& arguments, const json& variables) {
    return accumulate(arguments.begin(), arguments.end(), json::object(), [&](auto obj, const auto& arg) {
        obj[arg.name] = arg.Value(variables);
        return obj;
    });
}

}
