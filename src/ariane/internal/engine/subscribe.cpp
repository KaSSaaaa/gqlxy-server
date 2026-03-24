#include "subscribe.h"

#include <ariane/ResolverArgs.h>
#include <ariane/internal/ast/Selection.h>
#include <ariane/internal/engine/ResolveArguments.h>
#include <ariane/internal/peg/parser/query/ParseDocument.h>
#include <ariane/internal/utils/ranges.h>
#include <ariane/internal/utils/visit.h>
#include <format>
#include <nlohmann/json.hpp>

using namespace std;

namespace ariane::graphql::internal {

static SubscriptionHandle CreateErrorHandle(const string& message) {
    return SubscriptionHandle::SingleShot(ResolveResult {
        .errors = FieldErrors{
            {.message = message}
        }
    });
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
            .data = nlohmann::json {
                {outputKey, resolvedJson}
            },
            .errors = fieldErrors.empty() ? optional<FieldErrors>{} : fieldErrors
        };
    } catch (const exception& e) {
        return ResolveResult{
            .errors = FieldErrors{
                {.message = e.what(), .path = {outputKey}}
            }
        };
    }
}

static variant<string, Field> GetSubscriptionRootField(const Document& document, const ResolveQueryArgs& args) {
    auto operation = find_optional(document.operations, [](const auto& op) {
        return op.type._value == OperationType::SUBSCRIPTION;
    });
    if (!operation.has_value())
        return "No subscription operation found";

    auto rootFields = to_vector(FlattenSelections(operation->selectionSet, document.fragments, args.directives, args.variables)
        | views::filter([](const Field& f) {
            return f.name != "__typename";
        }));

    if (rootFields.empty())
        return "Subscription must have at least one root field";
    if (rootFields.size() > 1)
        return "Subscription must have exactly one root field (excluding __typename)";

    return rootFields.front();
}

static variant<string, SubscriptionResolver> GetSubscriptionResolver(const Field& rootField, const ResolveQueryArgs& args) {
    if (!args.resolvers.contains("Subscription"))
        return "No Subscription resolver registered";

    auto subTypeResolver = args.resolvers.at("Subscription").AsIf<Resolver>();
    if (!subTypeResolver || !subTypeResolver->contains(rootField.name))
        return format("No subscription resolver for field: {}", rootField.name);

    auto subResolver = subTypeResolver->at(rootField.name).AsIf<SubscriptionResolver>();
    if (!subResolver)
        return format("Field '{}' is not a SubscriptionResolver", rootField.name);

    return *subResolver;
}

SubscriptionHandle Subscribe(const ResolveQueryArgs& args) {
    auto document = ParseDocument(args.query);
    if (document.operations.empty())
        return CreateErrorHandle("Failed to parse subscription query");

    return visit(overloaded{
        [&](const Field& field) -> SubscriptionHandle {
            return visit(overloaded{
                [&](const SubscriptionResolver& subResolver) -> SubscriptionHandle {
                    auto stream = subResolver(ResolverArgs({
                        .args = ResolveArguments(field.arguments, args.variables),
                        .context = args.context
                    }));
                    if (!stream.Valid())
                        return CreateErrorHandle("Subscription resolver returned invalid stream");

                    auto state = make_shared<SubscribeState>(SubscribeState{
                        .stream = std::move(stream),
                        .queryArgs = args,
                        .rootField = field,
                        .fieldTypeName = FieldTypeName("Subscription", field.name, args.schemaDefinition),
                        .fragments = document.fragments,
                    });

                    return {
                        [state]() { return ResolveEvent(*state); },
                        [state]() { state->stream.Close(); }
                    };
                },
                [](const string& error) -> SubscriptionHandle {
                    return CreateErrorHandle(error);
                },
            }, GetSubscriptionResolver(field, args));
        },
        [](const string& error) -> SubscriptionHandle {
            return CreateErrorHandle(error);
        },
    }, GetSubscriptionRootField(document, args));
}

}
