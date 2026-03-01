#include "resolve.h"

#include <ariane/internal/introspection/types/SchemaDefinition.h>
#include <ariane/internal/peg/parser/query/ParseDocument.h>
#include <ariane/internal/utils/optional.h>
#include <ariane/internal/utils/visit.h>
#include <ariane/resolvers.h>
#include <ariane/schema.h>
#include <ariane/task.h>

#include <nlohmann/json.hpp>

using namespace std;

namespace ariane::graphql::internal {

using FragmentLookup = unordered_map<string, FragmentDefinition>;
using FieldErrors = vector<FieldError>;
using Path = vector<string>;

static vector<Field> FlattenSelections(const SelectionSet& ss, const FragmentLookup& frags,
                                       const optional<string>& concreteType = nullopt) {
    vector<Field> fields;
    for (const auto& sel : ss.selections) {
        visit(overloaded{
           [&](const Field& f) { fields.push_back(f); },
           [&](const FragmentSpread& s) {
               if (!frags.contains(s.name))
                   return;
               auto& fragment = frags.at(s.name);
               if (concreteType && fragment.typeCondition != *concreteType)
                   return;
               auto nested = FlattenSelections(fragment.selectionSet, frags, concreteType);
               fields.insert(fields.end(), nested.begin(), nested.end());
           },
           [&](const InlineFragment& i) {
               if (!i.selectionSet)
                   return;
               if (concreteType && i.typeCondition.has_value() && *i.typeCondition != *concreteType)
                   return;
               auto nested = FlattenSelections(*i.selectionSet, frags, concreteType);
               fields.insert(fields.end(), nested.begin(), nested.end());
           },
        }, sel);
    }
    return fields;
}

static nlohmann::json ResolveArguments(const vector<Argument>& args, const nlohmann::json& variables) {
    auto obj = nlohmann::json::object();
    for (const auto& arg : args) {
        if (!arg.value.empty() && arg.value[0] == '$') {
            auto varName = arg.value.substr(1);
            obj[arg.name] = variables.contains(varName) ? variables[varName] : nullptr;
        } else {
            try {
                obj[arg.name] = nlohmann::json::parse(arg.value);
            } catch (...) {
                obj[arg.name] = arg.value;
            }
        }
    }
    return obj;
}

static optional<string> ResolveType(const Resolver& rootResolver, const Resolver& current, const string& typeName) {
    if (typeName.empty() || !rootResolver.contains(typeName))
        return nullopt;

    auto* typeEntry = get_if<Resolver>(&rootResolver.at(typeName));
    if (!typeEntry || !typeEntry->contains("__resolveType"))
        return nullopt;

    auto& resolveType = typeEntry->at("__resolveType");
    if (holds_alternative<string>(resolveType))
        return get<string>(resolveType);

    if (!holds_alternative<TypeResolver>(resolveType))
        return nullopt;

    return get<TypeResolver>(resolveType)(current);
}

static string FieldTypeName(const string& typeName, const string& fieldName,
                             const SchemaDefinition& schemaDefinition) {
    if (typeName.empty() || !schemaDefinition.types.contains(typeName))
        return "";
    for (const auto& f : schemaDefinition.types.at(typeName).fields) {
        if (f.name != fieldName)
            continue;
        const TypeRef* typeRef = &f.type;
        while (typeRef && (typeRef->kind._value == TypeRefKind::NON_NULL || typeRef->kind._value == TypeRefKind::LIST))
            typeRef = typeRef->ofType ? typeRef->ofType.get() : nullptr;
        return typeRef ? typeRef->name : "";
    }
    return "";
}

Task<nlohmann::json> Resolve(const Resolver& rootResolver,
                             const ValueResolver& resolver,
                             const ResolverArgs& args,
                             const SelectionSet* selectionSet,
                             const string& typeName,
                             const SchemaDefinition& schemaDefinition,
                             const FragmentLookup& fragments,
                             const nlohmann::json& variables,
                             FieldErrors& fieldErrors,
                             Path path) {
    co_return co_await visit(
        overloaded{
            [](int v) -> Task<nlohmann::json> { co_return v; },
            [](uint64_t v) -> Task<nlohmann::json> { co_return v; },
            [](double v) -> Task<nlohmann::json> { co_return v; },
            [](float v) -> Task<nlohmann::json> { co_return v; },
            [](bool v) -> Task<nlohmann::json> { co_return v; },
            [](const string& v) -> Task<nlohmann::json> { co_return v; },
            [](monostate) -> Task<nlohmann::json> { co_return nullptr; },
            [&](const Resolver& currentResolver) -> Task<nlohmann::json> {
                auto resolvedType = ResolveType(rootResolver, currentResolver, typeName);
                auto obj = nlohmann::json::object();
                if (selectionSet == nullptr)
                    co_return obj;

                for (const auto& field : FlattenSelections(*selectionSet, fragments, resolvedType)) {
                    const auto& outputKey = field.alias.value_or(field.name);
                    if (field.name == "__typename") {
                        if (!resolvedType.has_value())
                            throw runtime_error("__resolveType returned nullopt for abstract type: " + typeName);
                        obj[outputKey] = resolvedType;
                        continue;
                    }
                    auto childPath = path;
                    childPath.push_back(outputKey);
                    try {
                        if (!currentResolver.contains(field.name))
                            throw runtime_error("Unknown property " + field.name);

                        obj[outputKey] = co_await Resolve(
                            rootResolver,
                            currentResolver.at(field.name),
                            ResolverArgs{
                                .args = ResolveArguments(field.arguments, variables)
                            },
                            field.selectionSet.get(),
                            FieldTypeName(typeName, field.name, schemaDefinition),
                            schemaDefinition,
                            fragments,
                            variables,
                            fieldErrors,
                            childPath);
                    } catch (const exception& e) {
                        obj[outputKey] = nullptr;
                        fieldErrors.push_back(FieldError {
                            .message = e.what(),
                            .path = childPath
                        });
                    }
                }
                co_return obj;
            },
            [&](const vector<ValueResolver>& vec) -> Task<nlohmann::json> {
                nlohmann::json arr = nlohmann::json::array();
                for (size_t i = 0; i < vec.size(); i++) {
                    auto elemPath = path;
                    elemPath.push_back(to_string(i));
                    try {
                        arr.push_back(co_await Resolve(
                            rootResolver,
                            vec[i],
                            {},
                            selectionSet,
                            typeName,
                            schemaDefinition,
                            fragments,
                            variables,
                            fieldErrors,
                            elemPath));
                    } catch (const exception& e) {
                        arr.push_back(nullptr);
                        fieldErrors.push_back({.message = e.what(), .path = elemPath});
                    }
                }
                co_return arr;
            },
            [&](const FunctionResolver& func) -> Task<nlohmann::json> {
                co_return co_await Resolve(rootResolver, func(args), {}, selectionSet, typeName, schemaDefinition,
                                           fragments, variables, fieldErrors, path);
            },
            [&](const AsyncFunctionResolver& func) -> Task<nlohmann::json> {
                co_return co_await Resolve(rootResolver, func(args).get(), {}, selectionSet, typeName,
                                           schemaDefinition, fragments, variables, fieldErrors, path);
            },
            [&](const CoroutineResolver& func) -> Task<nlohmann::json> {
                co_return co_await Resolve(rootResolver, co_await func(args), {}, selectionSet, typeName,
                                           schemaDefinition, fragments, variables, fieldErrors, path);
            },
            [&](const CallbackResolver& func) -> Task<nlohmann::json> {
                promise<ValueResolver> p;
                func(args, [&p](const auto& res) { p.set_value(res); });
                co_return co_await Resolve(rootResolver, p.get_future().get(), {}, selectionSet, typeName,
                                           schemaDefinition, fragments, variables, fieldErrors, path);
            },
            [&](const TypeResolver&) -> Task<nlohmann::json> { co_return nullptr; }
         },
         resolver);
}

Task<ResolveResult> ResolveOperations(ResolveQueryArgs args) {
    try {
        auto document = ParseDocument(args.query);

        if (document.operations.empty() && args.query.find_first_not_of(" \t\n\r") != string::npos) {
            co_return ResolveResult {
                .errors = FieldErrors{{.message = "Failed to parse query"}}
            };
        }

        nlohmann::json data = nlohmann::json::object();
        FieldErrors fieldErrors;

        for (const auto& op : document.operations) {
            string resolverType;
            if (op.type._value == OperationType::QUERY)
                resolverType = "Query";
            else if (op.type._value == OperationType::MUTATION)
                resolverType = "Mutation";
            else if (op.type._value == OperationType::SUBSCRIPTION)
                resolverType = "Subscription";

            if (!args.resolvers.contains(resolverType))
                continue;

            auto& typeResolver = args.resolvers.at(resolverType);
            if (!holds_alternative<Resolver>(typeResolver))
                continue;

            auto& fieldResolvers = get<Resolver>(typeResolver);
            for (const auto& field : FlattenSelections(op.selectionSet, document.fragments)) {
                const auto& outputKey = field.alias.value_or(field.name);
                if (!fieldResolvers.contains(field.name))
                    continue;
                auto fieldTypeName = FieldTypeName(resolverType, field.name, args.schemaDefinition);
                try {
                    data[outputKey] = co_await Resolve(
                         args.resolvers,
                         fieldResolvers.at(field.name),
                         ResolverArgs{
                             .args = ResolveArguments(field.arguments, args.variables)
                         },
                         field.selectionSet.get(),
                         fieldTypeName.empty() ? resolverType : fieldTypeName,
                         args.schemaDefinition,
                         document.fragments,
                         args.variables,
                         fieldErrors,
                         {outputKey});
                } catch (const exception& e) {
                    data[outputKey] = nullptr;
                    fieldErrors.push_back(FieldError {
                        .message = e.what(),
                        .path = {outputKey}
                    });
                }
            }
        }

        co_return ResolveResult {
            .data = data.dump(),
            .errors = fieldErrors.empty() ? optional<FieldErrors>{} : fieldErrors
        };
    } catch (const exception& e) {
        co_return ResolveResult {
            .errors = FieldErrors{
                FieldError {
                    .message = e.what()
                }
            }
        };
    }
}

}
