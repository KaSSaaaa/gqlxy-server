#include "create_mcp_tools.h"

#include <format>
#include <gqlxy/core/parser/ast/operation_definition.h>
#include <gqlxy/core/parser/ast/selection.h>
#include <gqlxy/core/parser/ast/variable_definition.h>
#include <gqlxy/core/parser/introspection/types/type_ref.h>
#include <gqlxy/core/utils/optional.h>
#include <gqlxy/core/utils/ranges.h>
#include <gqlxy/server/definitions/field_definition.h>
#include <gqlxy/server/definitions/type_definition.h>
#include <gqlxy/server/mcp/mcp_policy.h>
#include <gqlxy/server/schema.h>
#include <ranges>

using namespace std;
using namespace gqlxy::mcp;
using namespace gqlxy::parser;
using namespace gqlxy::utils;

namespace gqlxy::internal {

static string JsonSchemaType(const TypeRef& typeRef) {
    const string& name = typeRef.TypeName();
    if (name == "Int") return "integer";
    if (name == "Float") return "number";
    if (name == "Boolean") return "boolean";
    if (name == "ID") return "string";
    return "string";
}

static bool HasDirective(const vector<Directive>& directives, const string& name) {
    return ranges::any_of(directives, [&](const auto& d) { return d.name == name; });
}

static optional<string> DirectiveArg(const vector<Directive>& directives,
                                     const string& directiveName,
                                     const string& argName) {
    for (const auto& d : directives) {
        if (d.name != directiveName) continue;
        for (const auto& a : d.args) {
            if (a.name != argName || a.value.empty()) continue;
            auto v = a.value;
            if (v.size() >= 2 && v.front() == '"' && v.back() == '"')
                v = v.substr(1, v.size() - 2);
            return v;
        }
    }
    return nullopt;
}

static bool ShouldInclude(const FieldDefinition& field, DefaultMcpPolicy policy) {
    if (HasDirective(field.directives, "allowMcp")) return true;
    if (HasDirective(field.directives, "hideMcp")) return false;
    return policy._value == DefaultMcpPolicy::Enabled;
}

static vector<string> ScalarFields(const SchemaDefinition& schema, const string& typeName) {
    auto type = schema.types.find(typeName);
    auto result = to_vector(
        vector(type != schema.types.end() ? type->second.fields : vector<FieldDefinition>())
        | views::filter([&](const auto& field) {
            auto it = schema.types.find(field.type.TypeName());
            return it == schema.types.end() || it->second.kind._value != TypeKind::OBJECT;
        })
        | views::transform([](const auto& f) { return f.name; }));

    if (result.empty()) return {"__typename"};
    return result;
}

static SelectionSet BuildSelectionSet(const SchemaDefinition& schema, const string& typeName) {
    return SelectionSet{
        .selections = to_vector(ScalarFields(schema, typeName)
            | views::transform([](const auto& name) {
                return Selection{Field{.name = name}};
            }))
    };
}

static OperationDefinition BuildOperation(
    const SchemaDefinition& schema, const string& rootTypeName, const FieldDefinition& field) {
    bool isMutation = rootTypeName == schema.mutationTypeName.value_or("Mutation");
    return OperationDefinition{
        .type = isMutation ? OperationType::MUTATION : OperationType::QUERY,
        .variableDefinitions = to_vector(field.args | views::transform([](const auto& arg) {
            return VariableDefinition{.name = arg.name, .type = arg.type};
        })),
        .selectionSet = SelectionSet{
            .selections = {
                Selection{Field{
                    .name = field.name,
                    .arguments = to_vector(field.args | views::transform([](const auto& arg) {
                        return Argument{.name = arg.name, .value = Argument::Reference(arg.name)};
                    })),
                    .selectionSet = BuildSelectionSet(schema, field.type.TypeName()),
                }}
            }
        }
    };
}

static McpTool BuildTool(
    Schema& schema, const string& typeName, const FieldDefinition& field) {
    return CreateGraphQLMcpTool({
        .schema = schema,
        .name = DirectiveArg(field.directives, "allowMcp", "name").value_or(format("{}_{}", typeName, field.name)),
        .description = or_else(DirectiveArg(field.directives, "allowMcp", "description"), [&]() {
            return field.description;
        }),
        .args = to_vector(field.args | views::transform([](const auto& arg) {
            return McpToolArg {
                .name = arg.name,
                .description = arg.description,
                .jsonSchemaType = JsonSchemaType(arg.type),
                .required = arg.type.kind._value == TypeRefKind::NON_NULL,
            };
        })),
        .query = format("{}", BuildOperation(schema.Definition(), typeName, field)),
    });
}

static vector<McpTool> ExtractFromRootType(
    Schema& schema, const string& typeName, DefaultMcpPolicy policy) {
    if (!schema.Definition().types.contains(typeName)) return {};
    return to_vector(schema.Definition().types.at(typeName).fields
        | views::filter([&](const auto& f) { return ShouldInclude(f, policy); })
        | views::transform([&](const auto& f) { return BuildTool(schema, typeName, f); }));
}

vector<McpTool> CreateMcpTools(Schema& schema, DefaultMcpPolicy policy) {
    if (policy._value == DefaultMcpPolicy::Disabled) return {};
    return concat(
        ExtractFromRootType(schema, schema.Definition().queryTypeName.value_or("Query"), policy),
        ExtractFromRootType(schema, schema.Definition().mutationTypeName.value_or("Mutation"), policy)
    );
}

}
