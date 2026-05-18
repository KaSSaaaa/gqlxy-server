#include "validate.h"

#include <gqlxy/server/definitions/field_definition.h>
#include <gqlxy/server/definitions/schema_definition.h>
#include <algorithm>
#include <format>
#include <gqlxy/core/parser/ast/document.h>
#include <gqlxy/core/parser/ast/fragment_definition.h>
#include <gqlxy/core/parser/ast/fragments.h>
#include <gqlxy/core/parser/ast/operation_definition.h>
#include <gqlxy/core/parser/ast/selection.h>
#include <gqlxy/core/utils/optional.h>
#include <gqlxy/core/utils/ranges.h>
#include <gqlxy/core/utils/visit.h>
#include <gqlxy/server/definitions/type_definition.h>
#include <nlohmann/json.hpp>
#include <string>
#include <unordered_set>

using namespace std;
using namespace gqlxy::parser;
using namespace gqlxy::utils;
using namespace gqlxy;
using namespace nlohmann;

namespace gqlxy::internal {

static vector<string> CollectVarRefs(const SelectionSet& ss, const Fragments& frags);

static auto extractVariableReferences = views::filter([](const auto& arg) { return arg.IsVariable(); })
             | views::transform([](const auto& arg) { return arg.Reference(); });

static vector<string> CollectFieldVariableReferences(const Field& f, const Fragments& frags) {
    return concat(
         f.arguments | extractVariableReferences,
         flat_map(f.directives, [&](const Directive& directive) {
             return directive.args | extractVariableReferences;
         }),
         and_then(f.selectionSet, [&](const auto& selectionSet) {
             return CollectVarRefs(selectionSet, frags);
         })
    );
}

static vector<string> CollectVarRefs(const SelectionSet& ss, const Fragments& frags) {
    return flat_map(ss.selections, [&](const auto& selection) -> vector<string> {
        return visit(overloaded{
                   [&](const Field& f) -> vector<string> {
                       return CollectFieldVariableReferences(f, frags);
                   },
                   [&](const FragmentSpread& s) -> vector<string> {
                       return frags.contains(s.name) ? CollectVarRefs(frags.at(s.name).selectionSet, frags) : vector<string>{};
                   },
                   [&](const InlineFragment& i) -> vector<string> {
                       return and_then(i.selectionSet, [&](const auto& selectionSet) {
                           return CollectVarRefs(selectionSet, frags);
                       });
                   },
              },
              selection);
    });
}

static GraphQLErrors ValidateVariableDeclarations(const OperationDefinition& op, const Fragments& frags) {
    auto declared = to_unordered_set(op.variableDefinitions | views::transform([](const auto& variableDefinition) {
        return variableDefinition.name;
    }));

    return to_vector(to_unordered_set(CollectVarRefs(op.selectionSet, frags))
        | views::filter([&](const auto& ref) { return !declared.contains(ref);})
        | views::transform([](const auto& ref) -> GraphQLError {
            return {
                .message = format(R"(Variable "${}" is not declared in the operation)", ref)
            };
        }));
}

static GraphQLErrors ValidateVariableValues(const OperationDefinition& op, const json& variables) {
    return to_vector(op.variableDefinitions
        | views::filter([&](const auto& variableDefinition) {
            return variableDefinition.type.kind._value == TypeRefKind::NON_NULL &&
                   !variableDefinition.defaultValue.has_value() &&
                   (!variables.contains(variableDefinition.name) || variables[variableDefinition.name].is_null());
        })
        | views::transform([](const auto& variableDefinition) -> GraphQLError {
            return {
                .message = format(R"(Variable "${}" of type "{}" was not provided)", variableDefinition.name, variableDefinition.type.ToString())
            };
        }));
}

static GraphQLErrors ValidateSelections(const vector<Selection>& selections,
                                      const string& typeName,
                                      const SchemaDefinition& schema,
                                      const vector<VariableDefinition>& varDefs,
                                      const json& variables,
                                      const Fragments& frags,
                                      vector<string> path);

static GraphQLErrors ValidateFieldArgumentsExist(const Field& field,
                                               const FieldDefinition& fieldDefinition,
                                               const string& typeName,
                                               const vector<string>& fieldPath) {
    return to_vector(field.arguments | views::filter([&](const auto& arg) {
        return ranges::all_of(fieldDefinition.args, [&](const auto& a) { return a.name != arg.name; });
    }) | views::transform([&](const auto& arg) -> GraphQLError{
        return {
            .message = format(R"(Unknown argument "{}" on field "{}.{}")", arg.name, typeName, field.name),
            .path = fieldPath
        };
    }));
}

static bool IsVariableProvided(const json& variables, const string& variable) {
    return variables.contains(variable) && !variables[variable].is_null();
}

static bool HasDefaultValue(const vector<VariableDefinition>& variableDefinitions, const string& variable) {
    auto var = find_optional(variableDefinitions, [&](const auto& v) { return v.name == variable; });
    return var && var->defaultValue.has_value();
}

static GraphQLErrors ValidateRequiredFieldArguments(const Field& field,
                                                  const FieldDefinition& fieldDefinition,
                                                  const vector<VariableDefinition>& variableDefinitions,
                                                  const json& variables,
                                                  const string& typeName,
                                                  const vector<string>& fieldPath) {
    auto requiredArgs = fieldDefinition.args | views::filter([](const auto& arg) {
        return arg.type.kind._value == TypeRefKind::NON_NULL && !arg.defaultValue.has_value();
    });
    return flat_map(requiredArgs, [&](const auto& arg) -> GraphQLErrors {
        auto fieldArg = find_optional(field.arguments, [&](const auto& a) { return a.name == arg.name; });
        if (!fieldArg.has_value())
            return {
                GraphQLError {
                    .message = format(R"(Argument "{}" of field "{}.{}" is required)", arg.name, typeName, field.name),
                    .path = fieldPath
                }
            };
        if (fieldArg->IsVariable()) {
            auto variable = fieldArg->Reference();
            if (!IsVariableProvided(variables, variable) && !HasDefaultValue(variableDefinitions, variable))
                return {
                    GraphQLError {
                        .message = format(R"(Argument "{}" of field "{}.{}" is required, but variable "{}" was not provided)", arg.name, typeName, field.name, fieldArg->value),
                        .path = fieldPath
                    }
                };
        }
        return {};
    });
}

static GraphQLErrors ValidateField(const Field& field,
                                 const string& typeName,
                                 const SchemaDefinition& schema,
                                 const vector<VariableDefinition>& varDefs,
                                 const json& variables,
                                 const Fragments& frags,
                                 const vector<string>& path) {
    auto fieldPath = path;
    fieldPath.push_back(field.alias.value_or(field.name));

    if (!schema.types.contains(typeName))
        return {};

    const auto& typeDef = schema.types.at(typeName);
    if (typeDef.kind._value != TypeKind::OBJECT && typeDef.kind._value != TypeKind::INTERFACE)
        return {};

    auto fieldDefinition = find_optional(typeDef.fields, [&](const auto& f) { return f.name == field.name; });
    if (!fieldDefinition.has_value())
        return {
            GraphQLError {
                .message = format(R"(Cannot query field "{}" on type "{}")", field.name, typeName),
                .path = fieldPath
            }
        };

    return concat(
        ValidateFieldArgumentsExist(field, *fieldDefinition, typeName, fieldPath),
        ValidateRequiredFieldArguments(field, *fieldDefinition, varDefs, variables, typeName, fieldPath),
        ValidateSelections(field.selectionSet ? field.selectionSet->selections : vector<Selection>{},
                           fieldDefinition->type.TypeName(), schema, varDefs, variables, frags, fieldPath)
    );
}

static GraphQLErrors ValidateSelections(const vector<Selection>& selections,
                                      const string& typeName,
                                      const SchemaDefinition& schema,
                                      const vector<VariableDefinition>& varDefs,
                                      const json& variables,
                                      const Fragments& frags,
                                      vector<string> path) {
    return flat_map(selections, [&](const auto& sel) -> GraphQLErrors {
        return visit(overloaded{
                   [&](const Field& f) -> GraphQLErrors {
                       if (f.name.starts_with("__"))
                           return GraphQLErrors{};
                       return ValidateField(f, typeName, schema, varDefs, variables, frags, path);
                   },
                   [&](const FragmentSpread& s) -> GraphQLErrors {
                       if (!frags.contains(s.name))
                           return GraphQLErrors{};
                       const auto& frag = frags.at(s.name);
                       return ValidateSelections(frag.selectionSet.selections, frag.typeCondition, schema, varDefs, variables, frags, path);
                   },
                   [&](const InlineFragment& i) -> GraphQLErrors {
                       if (!i.selectionSet) return {};
                       return ValidateSelections(i.selectionSet->selections, i.typeCondition.value_or(typeName), schema, varDefs, variables, frags, path);
                   },
              }, sel);
    });
}

static string RootTypeName(const OperationType& opType, const SchemaDefinition& schema) {
    if (opType._value == OperationType::MUTATION)
        return schema.mutationTypeName.value_or("Mutation");
    if (opType._value == OperationType::SUBSCRIPTION)
        return schema.subscriptionTypeName.value_or("Subscription");
    return schema.queryTypeName.value_or("Query");
}

GraphQLErrors ValidateDocument(const Document& document,
                             const SchemaDefinition& schema,
                             const json& variables) {
    return flat_map(document.operations, [&](const auto& op) {
        return concat(
            ValidateVariableDeclarations(op, document.fragments),
            ValidateVariableValues(op, variables),
            ValidateSelections(op.selectionSet.selections,
                               RootTypeName(op.type, schema),
                               schema, op.variableDefinitions,
                               variables, document.fragments, {})
        );
    });
}

}
