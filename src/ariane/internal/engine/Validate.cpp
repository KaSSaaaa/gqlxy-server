#include "Validate.h"

#include <ariane/internal/ast/Document.h>
#include <ariane/internal/ast/FragmentDefinition.h>
#include <ariane/internal/ast/Fragments.h>
#include <ariane/internal/ast/OperationDefinition.h>
#include <ariane/internal/ast/Selection.h>
#include <ariane/internal/introspection/types/FieldDefinition.h>
#include <ariane/internal/introspection/types/SchemaDefinition.h>
#include <ariane/internal/introspection/types/TypeDefinition.h>
#include <ariane/internal/utils/optional.h>
#include <ariane/internal/utils/ranges.h>
#include <ariane/internal/utils/visit.h>
#include <algorithm>
#include <format>
#include <nlohmann/json.hpp>
#include <string>
#include <unordered_set>

using namespace std;
using namespace ariane::graphql;
using namespace nlohmann;

namespace ariane::graphql::internal {

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

static FieldErrors ValidateVariableDeclarations(const OperationDefinition& op, const Fragments& frags) {
    auto declared = to_unordered_set(op.variableDefinitions | views::transform([](const auto& variableDefinition) {
        return variableDefinition.name;
    }));

    return to_vector(to_unordered_set(CollectVarRefs(op.selectionSet, frags))
        | views::filter([&](const auto& ref) { return !declared.contains(ref);})
        | views::transform([](const auto& ref) -> FieldError {
            return {
                .message = format(R"(Variable "${}" is not declared in the operation)", ref)
            };
        }));
}

static FieldErrors ValidateVariableValues(const OperationDefinition& op, const json& variables) {
    return to_vector(op.variableDefinitions
        | views::filter([&](const auto& variableDefinition) {
            return variableDefinition.type.kind._value == TypeRefKind::NON_NULL &&
                   !variableDefinition.defaultValue.has_value() &&
                   (!variables.contains(variableDefinition.name) || variables[variableDefinition.name].is_null());
        })
        | views::transform([](const auto& variableDefinition) -> FieldError {
            return {
                .message = format(R"(Variable "${}" of type "{}" was not provided)", variableDefinition.name, variableDefinition.type.ToString())
            };
        }));
}

static FieldErrors ValidateSelections(const vector<Selection>& selections,
                                      const string& typeName,
                                      const SchemaDefinition& schema,
                                      const vector<VariableDefinition>& varDefs,
                                      const json& variables,
                                      const Fragments& frags,
                                      vector<string> path);

static FieldErrors ValidateFieldArgumentsExist(const Field& field,
                                               const FieldDefinition& fieldDefinition,
                                               const string& typeName,
                                               const vector<string>& fieldPath) {
    return to_vector(field.arguments | views::filter([&](const auto& arg) {
        return ranges::all_of(fieldDefinition.args, [&](const auto& a) { return a.name != arg.name; });
    }) | views::transform([&](const auto& arg) -> FieldError{
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
    return and_then(find_optional(variableDefinitions, [&](const auto& v) { return v.name == variable; }),
                                       [](const auto& var) { return make_optional(var.defaultValue.has_value()); }).value_or(false);
}

static FieldErrors ValidateRequiredFieldArguments(const Field& field,
                                                  const FieldDefinition& fieldDefinition,
                                                  const vector<VariableDefinition>& variableDefinitions,
                                                  const json& variables,
                                                  const string& typeName,
                                                  const vector<string>& fieldPath) {
    auto requiredArgs = fieldDefinition.args | views::filter([](const auto& arg) {
        return arg.type.kind._value == TypeRefKind::NON_NULL && !arg.defaultValue.has_value();
    });
    return flat_map(requiredArgs, [&](const auto& arg) -> FieldErrors {
        auto fieldArg = find_optional(field.arguments, [&](const auto& a) { return a.name == arg.name; });
        if (!fieldArg.has_value())
            return {
                FieldError {
                    .message = format(R"(Argument "{}" of field "{}.{}" is required)", arg.name, typeName, field.name),
                    .path = fieldPath
                }
            };
        if (fieldArg->IsVariable()) {
            auto variable = fieldArg->Reference();
            if (!IsVariableProvided(variables, variable) && !HasDefaultValue(variableDefinitions, variable))
                return {
                    FieldError {
                        .message = format(R"(Argument "{}" of field "{}.{}" is required, but variable "{}" was not provided)", arg.name, typeName, field.name, fieldArg->value),
                        .path = fieldPath
                    }
                };
        }
        return {};
    });
}

static FieldErrors ValidateField(const Field& field,
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
            FieldError {
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

static FieldErrors ValidateSelections(const vector<Selection>& selections,
                                      const string& typeName,
                                      const SchemaDefinition& schema,
                                      const vector<VariableDefinition>& varDefs,
                                      const json& variables,
                                      const Fragments& frags,
                                      vector<string> path) {
    return flat_map(selections, [&](const auto& sel) -> FieldErrors {
        return visit(overloaded{
                   [&](const Field& f) -> FieldErrors {
                       if (f.name.starts_with("__"))
                           return FieldErrors{};
                       return ValidateField(f, typeName, schema, varDefs, variables, frags, path);
                   },
                   [&](const FragmentSpread& s) -> FieldErrors {
                       if (!frags.contains(s.name))
                           return FieldErrors{};
                       const auto& frag = frags.at(s.name);
                       return ValidateSelections(frag.selectionSet.selections, frag.typeCondition, schema, varDefs, variables, frags, path);
                   },
                   [&](const InlineFragment& i) -> FieldErrors {
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

FieldErrors ValidateDocument(const Document& document,
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
