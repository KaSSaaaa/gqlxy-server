#include "subscribe.h"

#include <ariane/ResolverArgs.h>
#include <ariane/internal/ast/Selection.h>
#include <ariane/internal/engine/ResolveArguments.h>
#include <ariane/internal/peg/parser/query/ParseDocument.h>
#include <ariane/internal/utils/ranges.h>
#include <format>
#include <nlohmann/json.hpp>

using namespace std;

namespace ariane::graphql::internal {

static SubscriptionHandle CreateErrorHandle(const string& message) {
    auto fired = make_shared<bool>(false);
    return {
        [fired, message]() -> optional<ResolveResult> {
            if (*fired) return nullopt;
            *fired = true;
            return ResolveResult {
                .errors = FieldErrors{{.message = message}}
            };
        },
        []() {}
    };
}

struct SubscribeState {
    SubscriptionEventStream stream;
    ResolveQueryArgs queryArgs;
    Field rootField;
    optional<string> fieldTypeName;
    Fragments fragments;
};

static optional<ResolveResult> ResolveEvent(SubscribeState& s) {
    auto event = s.stream.Next();
    if (event.IsNull()) return nullopt;

    const auto outputKey = s.rootField.alias.value_or(s.rootField.name);

    try {
        FieldErrors fieldErrors;
        auto resolvedJson = Resolve(
            s.queryArgs,
            event,
            ResolverArgs({.context = s.queryArgs.context}),
            s.rootField.selectionSet,
            s.fieldTypeName,
            s.fragments,
            fieldErrors,
            {outputKey}).get();

        return ResolveResult{
            .data = nlohmann::json{
                {outputKey, resolvedJson}
            }.dump(),
            .errors = fieldErrors.empty() ? optional<FieldErrors>{} : fieldErrors
        };
    } catch (const exception& e) {
        return ResolveResult{
            .data = "null",
            .errors = FieldErrors{
                {.message = e.what(), .path = {outputKey}}
            }
        };
    }
}

//TODO strealk
SubscriptionHandle Subscribe(const ResolveQueryArgs& args) {
    auto document = ParseDocument(args.query);
    if (document.operations.empty())
        return CreateErrorHandle("Failed to parse subscription query");

    auto it = ranges::find_if(document.operations, [](const auto& op) {
        return op.type._value == OperationType::SUBSCRIPTION;
    });
    if (it == document.operations.end())
        return CreateErrorHandle("No subscription operation found");

    const auto& op = *it;
    auto rootFields = to_vector(
        FlattenSelections(op.selectionSet, document.fragments, args.directives, args.variables)
        | views::filter([](const Field& f) { return f.name != "__typename"; }));

    if (rootFields.empty())
        return CreateErrorHandle("Subscription must have at least one root field");
    if (rootFields.size() > 1)
        return CreateErrorHandle("Subscription must have exactly one root field (excluding __typename)");

    const auto& rootField = rootFields.front();

    if (!args.resolvers.contains("Subscription"))
        return CreateErrorHandle("No Subscription resolver registered");

    auto subTypeResolver = args.resolvers.at("Subscription").AsIf<Resolver>();
    if (!subTypeResolver.has_value() || !subTypeResolver->contains(rootField.name))
        return CreateErrorHandle(format("No subscription resolver for field: {}", rootField.name));

    auto subResolver = subTypeResolver->at(rootField.name).AsIf<SubscriptionResolver>();
    if (!subResolver.has_value())
        return CreateErrorHandle(format("Field '{}' is not a SubscriptionResolver", rootField.name));

    auto stream = subResolver.value()(ResolverArgs({
        .args = ResolveArguments(rootField.arguments, args.variables),
        .context = args.context
    }));
    if (!stream.Valid())
        return CreateErrorHandle("Subscription resolver returned invalid stream");

    auto state = make_shared<SubscribeState>(SubscribeState{
        .stream = std::move(stream),
        .queryArgs = args,
        .rootField = rootField,
        .fieldTypeName = FieldTypeName("Subscription", rootField.name, args.schemaDefinition),
        .fragments = document.fragments,
    });

    return {
        [state]() { return ResolveEvent(*state); },
        [state]() { state->stream.Close(); }
    };
}

}
