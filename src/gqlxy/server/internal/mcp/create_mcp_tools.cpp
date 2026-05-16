#include "create_mcp_tools.h"

#include <format>
#include <gqlxy/core/parser/ast/fragment_definition.h>
#include <gqlxy/core/parser/ast/operation_definition.h>
#include <gqlxy/core/parser/ast/selection.h>
#include <gqlxy/core/parser/ast/variable_definition.h>
#include <gqlxy/core/parser/introspection/types/type_ref.h>
#include <gqlxy/core/parser/peg/parser/query/parse_document.h>
#include <gqlxy/core/utils/optional.h>
#include <gqlxy/core/utils/ranges.h>
#include <gqlxy/server/definitions/field_definition.h>
#include <gqlxy/server/definitions/type_definition.h>
#include <gqlxy/server/mcp/mcp_policy.h>
#include <gqlxy/server/mcp/mcp_tool.h>
#include <gqlxy/server/schema.h>
#include <ranges>

using namespace std;
using namespace gqlxy::mcp;
using namespace gqlxy::parser;
using namespace gqlxy::utils;
using namespace nlohmann;

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

static bool IsObjectType(const SchemaDefinition& schema, const string& typeName) {
    auto it = schema.types.find(typeName);
    return it != schema.types.end() && it->second.kind._value == TypeKind::OBJECT;
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

static string DefaultFragmentName(const string& typeName) {
    return format("{}Fragment", typeName);
}

static FragmentDefinition DefaultFragment(const SchemaDefinition& schema, const string& typeName) {
    return FragmentDefinition{
        .name = DefaultFragmentName(typeName),
        .typeCondition = typeName,
        .selectionSet = SelectionSet{
            .selections = to_vector(ScalarFields(schema, typeName)
                | views::transform([](const auto& name) {
                    return Selection{Field{.name = name}};
                }))
        }
    };
}

static optional<string> ParseFragmentName(const string& fragmentStr) {
    auto [_, fragments] = ParseDocument(fragmentStr);
    auto fragment = to_vector(fragments | views::values);
    return make_optional_if(!fragment.empty(), [&]() { return fragment.front().name; });
}

static OperationDefinition BuildOperation(
    const SchemaDefinition& schema, const string& rootTypeName,
    const FieldDefinition& field, const string& spreadName) {
    return OperationDefinition{
        .type = rootTypeName == schema.mutationTypeName.value_or("Mutation") ? OperationType::MUTATION
              : rootTypeName == schema.subscriptionTypeName.value_or("Subscription") ? OperationType::SUBSCRIPTION
              : OperationType::QUERY,
        .variableDefinitions = to_vector(field.args | views::transform([](const auto& arg) {
            return VariableDefinition{.name = arg.name, .type = arg.type};
        })),
        .selectionSet = SelectionSet{
            .selections = {Selection{Field{
                .name = field.name,
                .arguments = to_vector(field.args | views::transform([](const auto& arg) {
                     return Argument{.name = arg.name, .value = "$" + arg.name};
                 })),
                .selectionSet = make_optional_if(IsObjectType(schema, field.type.TypeName()), [spreadName]() {
                    return SelectionSet{.selections = {
                        Selection{FragmentSpread{.name = spreadName}}
                    }};
                })
            }}}
        }
    };
}

static string BuildQuery(
    const SchemaDefinition& schema, const string& rootTypeName, const FieldDefinition& field,
    const vector<string>& fragments) {
    const string& returnTypeName = field.type.TypeName();
    if (!IsObjectType(schema, returnTypeName)) return format("{}", BuildOperation(schema, rootTypeName, field, ""));
    if (!fragments.empty()) {
        return format("{}\n{}",
            BuildOperation(schema, rootTypeName, field, ParseFragmentName(fragments[0]).value_or(DefaultFragmentName(returnTypeName))),
            fragments | join_with("\n"));
    }
    auto defaultFrag = DefaultFragment(schema, returnTypeName);
    return format("{}\n{}", BuildOperation(schema, rootTypeName, field, defaultFrag.name), defaultFrag);
}

static vector<McpToolArg> BuildToolArgs(const SchemaDefinition& schema, const FieldDefinition& field) {
    auto args = to_vector(field.args | views::transform([](const auto& arg) {
        return McpToolArg{
            .name = arg.name,
            .description = arg.description,
            .jsonSchemaType = JsonSchemaType(arg.type),
            .required = arg.type.kind._value == TypeRefKind::NON_NULL,
        };
    }));
    if (IsObjectType(schema, field.type.TypeName())) {
        auto defaultFragment = format("{}", DefaultFragment(schema, field.type.TypeName()));
        args.push_back(McpToolArg{
            .name = "fragments",
            .description = format("GraphQL fragment definitions to use as the selection set. "
                                  R"(Defaults to: ["{}"])", defaultFragment),
            .jsonSchemaType = "array",
            .jsonSchemaItemType = "string",
            .required = false,
        });
    }
    return args;
}

static auto ParseFragments(const optional<string>& fragmentsJson) {
    vector<string> fragments;
    auto parsed = json::parse(fragmentsJson.value_or("[]"), nullptr, false);
    if (!parsed.is_array()) return fragments;
    return to_vector(parsed | views::transform([](const json& arg) { return arg.get<string>(); }));
}

static optional<string> FragmentsJson(const json& callArgs) {
    return make_optional_if(callArgs.contains("fragments") && callArgs["fragments"].is_array(), [&]() {
        return callArgs["fragments"].dump();
    });
}

static json BuildVariables(const json& callArgs, const json& staticVars) {
    json variables = staticVars;
    for (const auto& [key, value] :
         callArgs.items() | views::filter([](const auto& kvp) { return kvp.key() != "fragments"; }))
        variables[key] = value;
    return variables;
}

static McpTool BuildTool(Schema& schema, const string& typeName, const FieldDefinition& field) {
    return McpTool {
        .name = DirectiveArg(field.directives, "allowMcp", "name").value_or(format("{}_{}", typeName, field.name)),
        .description = or_else(DirectiveArg(field.directives, "allowMcp", "description"), [&]() {
            return field.description;
        }),
        .args = BuildToolArgs(schema.Definition(), field),
        .handler = [&schema, field, typeName](const json& callArgs) {
             SchemaResolveArgs args = {
                 .query = BuildQuery(schema.Definition(), typeName, field, ParseFragments(FragmentsJson(callArgs))),
                 .variables = BuildVariables(callArgs, json::object()),
             };
             return typeName == schema.Definition().subscriptionTypeName.value_or("Subscription")
                ? schema.Subscribe(args)
                : SubscriptionHandle::SingleShot(schema.Resolve(args).get());
        }
    };
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
        ExtractFromRootType(schema, schema.Definition().mutationTypeName.value_or("Mutation"), policy),
        ExtractFromRootType(schema, schema.Definition().subscriptionTypeName.value_or("Subscription"), policy)
    );
}

}
