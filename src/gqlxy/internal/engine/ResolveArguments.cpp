#include "ResolveArguments.h"

#include <gqlxy/internal/ast/Selection.h>
#include <gqlxy/internal/introspection/types/SchemaDefinition.h>

#include <algorithm>

#include "gqlxy/internal/utils/ranges.h"
#include "resolve.h"

using namespace std;
using namespace nlohmann;

namespace gqlxy::internal {

vector<InputValueDefinition> FieldArgDefs(const string& typeName, const string& fieldName,
                                                        const SchemaDefinition& schema) {
    if (!schema.types.contains(typeName))
        return {};

    return flat_map(
        schema.types.at(typeName).fields | views::filter([&](const auto& f) { return f.name == fieldName; }),
        [](const auto& f) { return f.args; }
    );
}

json ResolveArguments(const Field& field,
                      const ResolveQueryArgs& args,
                      const string& typeName) {
    auto& [query, variables, schemaDefinition, resolvers, directives, scalars, operationName, context] = args;
    auto argDefs = FieldArgDefs(typeName, field.name, schemaDefinition);

    return accumulate(field.arguments.begin(), field.arguments.end(), json::object(), [&](auto obj, const auto& arg) {
        auto value = arg.Value(variables);

        if (auto argDef = find_optional(argDefs, [&](const auto& d) { return d.name == arg.name; })) {
            if (scalars.contains(argDef->type.TypeName()))
                value = scalars.at(argDef->type.TypeName())(value);
        }

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
