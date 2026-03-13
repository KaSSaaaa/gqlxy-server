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
         concat(f.directives
             | views::transform([&](const Directive& directive) {
                 return directive.args | extractVariableReferences;
               })
             | views::join),
         and_then(f.selectionSet, [&](const auto& selectionSet) {
             return CollectVarRefs(selectionSet, frags);
         })
    );
}

static vector<string> CollectVarRefs(const SelectionSet& ss, const Fragments& frags) {
    return concat(ss.selections | views::transform([&](const auto& selection) {
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
    }) | views::join);
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

// TODO Cleanup
static FieldErrors ValidateRequiredFieldArguments(const Field& field,
                                                  const FieldDefinition& fieldDefinition,
                                                  const vector<VariableDefinition>& variableDefinitions,
                                                  const json& variables,
                                                  const string& typeName,
                                                  const vector<string>& fieldPath) {
    return to_vector(fieldDefinition.args | views::filter([](const auto& arg) {
        return arg.type.kind._value == TypeRefKind::NON_NULL && !arg.defaultValue.has_value();
    }) | views::transform([&](const auto& arg) -> optional<FieldError> {
        auto argIt = ranges::find_if(field.arguments, [&](const auto& a) { return a.name == arg.name; });
        if (argIt == field.arguments.end()) {
            return make_optional(FieldError {
                .message = format(R"(Argument "{}" of field "{}.{}" is required)", arg.name, typeName, field.name),
                .path = fieldPath
            });
        }
        if (argIt->IsVariable()) {
            const auto varName = argIt->Reference();
            auto varIt = ranges::find_if(variableDefinitions, [&](const auto& v) { return v.name == varName; });
            const bool varHasDefault = varIt != variableDefinitions.end() && varIt->defaultValue.has_value();
            const bool varProvided = variables.contains(varName) && !variables[varName].is_null();
            if (!varProvided && !varHasDefault)
                return make_optional(FieldError {
                    .message = format(R"(Argument "{}" of field "{}.{}" is required, but variable "{}" was not provided)", arg.name, typeName, field.name, argIt->value),
                    .path = fieldPath
                });
        }
        return nullopt;
    })
    | views::filter([](const auto& err) { return err.has_value(); })
    | views::transform([](const auto& err) { return err.value(); }));
}

// TODO Cleanup
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

    auto fieldIt = ranges::find_if(typeDef.fields, [&](const auto& f) { return f.name == field.name; });
    if (fieldIt == typeDef.fields.end()) {
        return {{
            .message = format(R"(Cannot query field "{}" on type "{}")", field.name, typeName),
            .path = fieldPath
        }};
    }

    return concat(
        ValidateFieldArgumentsExist(field, *fieldIt, typeName, fieldPath),
        ValidateRequiredFieldArguments(field, *fieldIt, varDefs, variables, typeName, fieldPath),
        ValidateSelections(or_else(and_then(field.selectionSet, [](const auto& ss) {
            return make_optional(ss.selections);
        }), []() {
            return vector<Selection>{};
        }).value(), fieldIt->type.TypeName(), schema, varDefs, variables, frags, fieldPath)
    );
}

// TODO Cleanup
static FieldErrors ValidateSelections(const vector<Selection>& selections,
                                      const string& typeName,
                                      const SchemaDefinition& schema,
                                      const vector<VariableDefinition>& varDefs,
                                      const json& variables,
                                      const Fragments& frags,
                                      vector<string> path) {
    return concat(selections | views::transform([&](const auto& sel) -> FieldErrors {
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
                       const auto fragType = i.typeCondition.value_or(typeName);
                       if (i.selectionSet)
                           return ValidateSelections(or_else(and_then(i.selectionSet, [](const auto& ss) {
                               return make_optional(ss.selections);
                           }), []() {
                               return vector<Selection>{};
                           }).value(), fragType, schema, varDefs, variables, frags, path);
                       return FieldErrors{};
                   },
              }, sel);
    }) | views::join);
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
    return to_vector(concat(document.operations | views::transform([&](const auto& op) {
        return concat(
            ValidateVariableDeclarations(op, document.fragments),
            ValidateVariableValues(op, variables),
            ValidateSelections(op.selectionSet.selections,
                               RootTypeName(op.type, schema),
                               schema, op.variableDefinitions,
                               variables, document.fragments, {})
        );
    }) | views::join));
}

}
