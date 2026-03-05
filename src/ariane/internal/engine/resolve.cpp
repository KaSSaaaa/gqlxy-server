#include "resolve.h"

#include <ariane/internal/ast/Fragments.h>
#include <ariane/internal/ast/Selection.h>
#include <ariane/internal/engine/ApplyDirectives.h>
#include <ariane/internal/engine/ResolveArguments.h>
#include <ariane/internal/introspection/types/SchemaDefinition.h>
#include <ariane/internal/json/JsonToValueResolver.h>
#include <ariane/internal/peg/parser/query/ParseDocument.h>
#include <ariane/internal/utils/optional.h>
#include <ariane/internal/utils/visit.h>
#include <ariane/resolvers.h>
#include <ariane/schema.h>
#include <ariane/task.h>
#include <nlohmann/json.hpp>

using namespace std;

namespace ariane::graphql::internal {

using Path = vector<string>;

static optional<string> ResolveType(const Resolver& rootResolver, const Resolver& current, const optional<string>& typeName) {
    if (!typeName.has_value() || !rootResolver.contains(typeName.value()))
        return nullopt;

    auto* typeEntry = get_if<Resolver>(&rootResolver.at(typeName.value()));
    if (!typeEntry || !typeEntry->contains("__resolveType"))
        return nullopt;

    auto& resolveType = typeEntry->at("__resolveType");
    if (holds_alternative<string>(resolveType))
        return get<string>(resolveType);

    if (!holds_alternative<TypeResolver>(resolveType))
        return nullopt;

    return get<TypeResolver>(resolveType)(current);
}

static optional<string> FieldTypeName(const optional<string>& typeName, const string& fieldName,
                                      const SchemaDefinition& schemaDefinition) {
    return and_then(typeName, [&](const auto& t) -> optional<string> {
        if (!schemaDefinition.types.contains(t))
            return nullopt;

        auto& fields = schemaDefinition.types.at(typeName.value()).fields;
        auto it = ranges::find_if(fields, [fieldName](const auto& field) { return field.name == fieldName; });
        return it != fields.end() ? make_optional(it->type.typeName()) : nullopt;
    });
}

Task<nlohmann::json> Resolve(const ResolveQueryArgs& args,
                             const ValueResolver& resolver,
                             const ResolverArgs& resolverArgs,
                             const optional<SelectionSet>& selectionSet,
                             const optional<string>& typeName,
                             const Fragments& fragments,
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
                auto resolvedType = ResolveType(args.resolvers, currentResolver, typeName);
                auto obj = nlohmann::json::object();
                if (!selectionSet.has_value())
                    co_return obj;

                for (const auto& field : FlattenSelections(*selectionSet, fragments, args.directives, args.variables, resolvedType)) {
                    const auto& outputKey = field.alias.value_or(field.name);
                    if (field.name == "__typename") {
                        if (!resolvedType.has_value())
                            throw runtime_error("__resolveType returned nullopt for abstract type: " + typeName.value_or("Unknown"));
                        obj[outputKey] = resolvedType;
                        continue;
                    }
                    auto childPath = path;
                    childPath.push_back(outputKey);
                    try {
                        if (!currentResolver.contains(field.name))
                            throw runtime_error("Unknown property " + field.name);

                        auto resolvedJson = co_await Resolve(
                            args,
                            currentResolver.at(field.name),
                            ResolverArgs{
                                .args = ResolveArguments(field.arguments, args.variables)
                            },
                            field.selectionSet,
                            FieldTypeName(typeName, field.name, args.schemaDefinition),
                            fragments,
                            fieldErrors,
                            childPath);

                        if (field.directives.empty()) {
                            obj[outputKey] = resolvedJson;
                            continue;
                        }
                        auto afterDirectives = ApplyDirectives(field.directives, args.directives, args.variables,
                                                               JsonToValueResolver(resolvedJson));
                        if (!afterDirectives.has_value())
                            continue;
                        if (holds_alternative<monostate>(*afterDirectives))
                            obj[outputKey] = resolvedJson;
                        else
                            obj[outputKey] = co_await Resolve(args, *afterDirectives, {}, nullopt, "",
                                                              fragments, fieldErrors, childPath);
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
                            args,
                            vec[i],
                            {},
                            selectionSet,
                            typeName,
                            fragments,
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
                co_return co_await Resolve(args, func(resolverArgs), {}, selectionSet, typeName,
                                           fragments, fieldErrors, path);
            },
            [&](const AsyncFunctionResolver& func) -> Task<nlohmann::json> {
                co_return co_await Resolve(args, func(resolverArgs).get(), {}, selectionSet, typeName,
                                           fragments, fieldErrors, path);
            },
            [&](const CoroutineResolver& func) -> Task<nlohmann::json> {
                co_return co_await Resolve(args, co_await func(resolverArgs), {}, selectionSet, typeName,
                                           fragments, fieldErrors, path);
            },
            [&](const CallbackResolver& func) -> Task<nlohmann::json> {
                promise<ValueResolver> p;
                func(resolverArgs, [&p](const auto& res) { p.set_value(res); });
                co_return co_await Resolve(args, p.get_future().get(), {}, selectionSet, typeName,
                                           fragments, fieldErrors, path);
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
            for (const auto& field : FlattenSelections(op.selectionSet, document.fragments, args.directives, args.variables)) {
                const auto& outputKey = field.alias.value_or(field.name);
                if (!fieldResolvers.contains(field.name))
                    continue;
                auto fieldTypeName = FieldTypeName(resolverType, field.name, args.schemaDefinition);
                try {
                    auto resolvedJson = co_await Resolve(
                         args,
                         fieldResolvers.at(field.name),
                         ResolverArgs{
                             .args = ResolveArguments(field.arguments, args.variables)
                         },
                         field.selectionSet,
                         fieldTypeName.value_or(resolverType),
                         document.fragments,
                         fieldErrors,
                         {outputKey});

                    if (field.directives.empty()) {
                        data[outputKey] = resolvedJson;
                    } else if (auto afterDirectives = ApplyDirectives(field.directives,
                                                                      args.directives,
                                                                      args.variables,
                                                                      JsonToValueResolver(resolvedJson));
                               afterDirectives.has_value()) {
                        data[outputKey] = holds_alternative<monostate>(*afterDirectives)
                            ? resolvedJson
                            : co_await Resolve(args,
                                               *afterDirectives,
                                               {}, nullopt, "",
                                               document.fragments,
                                               fieldErrors,
                                               {outputKey});
                    }
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
