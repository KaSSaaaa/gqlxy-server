#include "mcp_tool_registry.h"

#include <gqlxy/server/definitions/field_definition.h>
#include <gqlxy/server/definitions/schema_definition.h>
#include <gqlxy/server/definitions/type_definition.h>
#include <format>
#include <gqlxy/core/results.h>
#include <gqlxy/server/schema.h>

using namespace std;
using namespace std::views;
using namespace gqlxy::internal;
using namespace gqlxy::utils;
using namespace nlohmann;

namespace gqlxy::mcp {

McpToolRegistry::McpToolRegistry(Schema& schema, const vector<McpTool>& tools)
    : _schema(schema),
      _tools(tools) {}

bool McpToolRegistry::IsEmpty() const {
    return _tools.empty();
}

vector<McpTool> McpToolRegistry::Tools() const {
    return _tools;
}

static vector<string> Fields(const SchemaDefinition& schema, const string& typeName) {
    auto type = schema.types.find(typeName);
    auto result = to_vector(vector(type != schema.types.end() ? type->second.fields : vector<FieldDefinition>())
        | filter([&](const auto& field) {
            auto it = schema.types.find(field.type.TypeName());
            return it == schema.types.end() || it->second.kind._value != TypeKind::OBJECT;
        })
        | views::transform([](const auto& f) { return f.name; }));

    if (result.empty()) return {"__typename"};
    return result;
}

static string BuildSelectionSet(const SchemaDefinition& schema, const string& typeName) {
    return format(R"({{ {} }})", Fields(schema, typeName) | join_with(" "));
}

static string BuildGraphQLOperation(
    const SchemaDefinition& schema, const string& rootTypeName, const FieldDefinition& field, const json& args) {
    string varDecls, argPassing;
    auto arguments = field.args | filter([&](const auto& a) { return args.contains(a.name); });

    return format(R"({0}{1} {{ {2}{3} {4} }})",
        rootTypeName == schema.mutationTypeName.value_or("Mutation") ? "mutation" : "query",
        !arguments.empty() ? format(" ({})", arguments | views::transform([](const auto& arg) {
            return format("${}: {} ", arg.name, arg.type.ToString());
        }) | join_with("")) : "",
        field.name,
        !arguments.empty() ? format("({})", arguments | views::transform([](const auto& arg) {
            return format("{}: ${} ", arg.name, arg.name);
        }) | join_with("")) : "",
        BuildSelectionSet(schema, field.type.TypeName())
    );
}

ToolCallResult McpToolRegistry::Call(const string& toolName, const json& args) const {
    const auto& schemaDef = _schema.Definition();
    auto sep = toolName.find('_');
    if (sep == string::npos) return {.isError = true};

    string rootTypeName = toolName.substr(0, sep);
    string fieldName = toolName.substr(sep + 1);

    if (!schemaDef.types.contains(rootTypeName)) return {.isError = true};

    const auto& rootType = schemaDef.types.at(rootTypeName);
    auto it = ranges::find_if(rootType.fields, [&](const auto& f) { return f.name == fieldName; });
    if (it == rootType.fields.end()) return {.isError = true};

    string query = BuildGraphQLOperation(schemaDef, rootTypeName, *it, args);

    json variables = json::object();
    for (const auto& arg : it->args)
        if (args.contains(arg.name)) variables[arg.name] = args[arg.name];

    auto result = _schema.Resolve({.query = query, .variables = variables}).get();
    return {.data = Serialize(result), .isError = false};
}

}
