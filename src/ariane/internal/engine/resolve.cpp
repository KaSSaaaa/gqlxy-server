#include "resolve.h"
#include <ariane/internal/ast/Fragments.h>
#include <ariane/internal/ast/Selection.h>
#include <ariane/internal/engine/ApplyDirectives.h>
#include <ariane/internal/engine/ResolveArguments.h>
#include <ariane/internal/engine/Validate.h>
#include <ariane/internal/introspection/types/SchemaDefinition.h>
#include <ariane/internal/json/JsonToValueResolver.h>
#include <ariane/internal/peg/parser/query/ParseDocument.h>
#include <ariane/internal/utils/expect.h>
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

    auto typeEntry = rootResolver.at(typeName.value()).AsIf<Resolver>();
    if (!typeEntry.has_value() || !typeEntry->contains("__resolveType"))
        return nullopt;

    auto& resolveType = typeEntry->at("__resolveType");
    return or_else(and_then(resolveType.AsIf<string>(), [](const auto& resolveType) {
        return make_optional(resolveType);
    }), [&]() {
        return and_then(resolveType.AsIf<TypeResolver>(), [&](const auto& t) {
            return t(current);
        });
    });
}

optional<string> FieldTypeName(const optional<string>& typeName, const string& fieldName,
                               const SchemaDefinition& schemaDefinition) {
    return and_then(typeName, [&](const auto& t) -> optional<string> {
        if (!schemaDefinition.types.contains(t))
            return nullopt;

        auto& fields = schemaDefinition.types.at(typeName.value()).fields;
        auto it = ranges::find_if(fields, [fieldName](const auto& field) { return field.name == fieldName; });
        return it != fields.end() ? make_optional(it->type.TypeName()) : nullopt;
    });
}

Task<nlohmann::json> Resolve(const ResolveQueryArgs& args,
                             const ValueResolver& resolver,
                             const ResolverArgs& resolverArgs,
                             const optional<SelectionSet>& selectionSet,
                             const optional<string>& typeName,
                             const Fragments& fragments,
                             FieldErrors& fieldErrors,
                             const Path& path) {
    auto resolve = [&](const ValueResolver& currentResolver,
                       const optional<SelectionSet>& fieldSelectionSet,
                       const optional<string>& fieldTypeName,
                       const Path& fieldPath,
                       const ResolverArgs& currentArgs = ResolverArgs()) {
        return Resolve(args, currentResolver, currentArgs, fieldSelectionSet, fieldTypeName, fragments, fieldErrors, fieldPath);
    };
    co_return co_await visit(
        overloaded{
            [](int v) -> Task<nlohmann::json> { co_return v; },
            [](uint64_t v) -> Task<nlohmann::json> { co_return v; },
            [](double v) -> Task<nlohmann::json> { co_return v; },
            [](float v) -> Task<nlohmann::json> { co_return v; },
            [](bool v) -> Task<nlohmann::json> { co_return v; },
            [](const string& v) -> Task<nlohmann::json> { co_return v; },
            [](const TypeResolver&) -> Task<nlohmann::json> { co_return nullptr; },
            [](const SubscriptionResolver&) -> Task<nlohmann::json> { co_return nullptr; },
            [](const ScalarType& s) -> Task<nlohmann::json> { co_return s.serialize(); },
            [](monostate) -> Task<nlohmann::json> { co_return nullptr; },
            [&](const Resolver& currentResolver) -> Task<nlohmann::json> {
                auto resolvedType = ResolveType(args.resolvers, currentResolver, typeName);
                auto obj = nlohmann::json::object();
                if (!selectionSet.has_value())
                    co_return obj;

                for (const auto& field : FlattenSelections(*selectionSet, fragments, args.directives, args.variables, resolvedType)) {
                    const auto& outputKey = field.alias.value_or(field.name);
                    if (field.name == "__typename") {
                        expect(resolvedType.has_value(), format("__resolveType returned nullopt for abstract type: {}", typeName.value_or("Unknown")));
                        obj[outputKey] = resolvedType;
                        continue;
                    }
                    auto childPath = path;
                    childPath.push_back(outputKey);
                    try {
                        expect(currentResolver.contains(field.name), format("Unknown property {}", field.name));

                        auto resolvedJson = co_await resolve(
                             currentResolver.at(field.name),
                             field.selectionSet,
                             FieldTypeName(typeName, field.name, args.schemaDefinition),
                             childPath,
                             ResolverArgs({
                                  .args = ResolveArguments(field.arguments, args.variables),
                                  .context = args.context,
                             }));

                        if (field.directives.empty()) {
                            obj[outputKey] = resolvedJson;
                            continue;
                        }
                        auto afterDirectives = ApplyDirectives(field.directives, args.directives, args.variables,
                                                               JsonToValueResolver(resolvedJson));
                        if (!afterDirectives.has_value())
                            continue;

                        obj[outputKey] = !afterDirectives->IsNull()
                            ? co_await resolve(*afterDirectives, nullopt, nullopt, childPath)
                            : resolvedJson;
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
                        arr.push_back(co_await resolve(vec[i], selectionSet, typeName, elemPath));
                    } catch (const exception& e) {
                        arr.push_back(nullptr);
                        fieldErrors.push_back({.message = e.what(), .path = elemPath});
                    }
                }
                co_return arr;
            },
            [&](const FunctionResolver& func) -> Task<nlohmann::json> {
                co_return co_await resolve(func(resolverArgs), selectionSet, typeName, path);
            },
            [&](const AsyncFunctionResolver& func) -> Task<nlohmann::json> {
                co_return co_await resolve(func(resolverArgs).get(), selectionSet, typeName, path);
            },
            [&](const CoroutineResolver& func) -> Task<nlohmann::json> {
                co_return co_await resolve(co_await func(resolverArgs), selectionSet, typeName, path);
            },
            [&](const CallbackResolver& func) -> Task<nlohmann::json> {
                promise<ValueResolver> p;
                func(resolverArgs, [&p](const auto& res) { p.set_value(res); });
                co_return co_await resolve(p.get_future().get(), selectionSet, typeName, path);
            },
         },
         resolver);
}

Task<ResolveResult> ResolveOperations(const ResolveQueryArgs& args) {
    try {
        auto document = ParseDocument(args.query);

        if (document.operations.empty() && args.query.find_first_not_of(" \t\n\r") != string::npos) {
            co_return ResolveResult {
                .errors = FieldErrors{{.message = "Failed to parse query"}}
            };
        }

        if (!args.operationName.empty()) {
            auto it = ranges::find_if(document.operations, [&](const auto& op) {
                return op.name.has_value() && op.name.value() == args.operationName;
            });
            if (it == document.operations.end()) {
                co_return ResolveResult {
                    .errors = FieldErrors{{.message = "Unknown operation: " + args.operationName}}
                };
            }
            document.operations = {*it};
        } else if (document.operations.size() > 1) {
            co_return ResolveResult {
                .errors = FieldErrors{{.message = "Must provide operationName when document contains multiple operations"}}
            };
        }

        if (auto errs = ValidateDocument(document, args.schemaDefinition, args.variables); !errs.empty())
            co_return ResolveResult{.errors = errs};

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
            if (!typeResolver.Is<Resolver>())
                continue;

            auto& fieldResolvers = typeResolver.As<Resolver>();
            for (const auto& field : FlattenSelections(op.selectionSet, document.fragments, args.directives, args.variables)) {
                const auto& outputKey = field.alias.value_or(field.name);
                if (!fieldResolvers.contains(field.name))
                    continue;
                auto fieldTypeName = FieldTypeName(resolverType, field.name, args.schemaDefinition);
                try {
                    auto resolvedJson = co_await Resolve(
                         args,
                         fieldResolvers.at(field.name),
                         ResolverArgs({
                             .args = ResolveArguments(field, args, resolverType),
                             .context = args.context,
                         }),
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
                        data[outputKey] = afterDirectives->IsNull()
                            ? resolvedJson
                            : co_await Resolve(args,
                                               *afterDirectives,
                                               ResolverArgs(), nullopt, "",
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
