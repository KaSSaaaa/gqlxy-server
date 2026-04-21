---
title: Subscriptions
---

GQLXY supports GraphQL subscriptions via its built-in `PubSub` system. Each published event flows through the full field-execution pipeline and produces a standard `GraphQLResponse`.

## Defining a subscription

Add a `Subscription` type to your SDL and a `SubscriptionResolver` to your resolver map:

```cpp
#include <gqlxy/pubsub.h>
#include <gqlxy/schema.h>
#include <gqlxy/subscription.h>

using namespace gqlxy;

PubSub pubsub;

Schema schema({
    .typeDefs = R"(
        type Subscription {
            messageAdded: Message
        }
        
        type Message {
            text: String
            author: String
        }
    )",
    .resolvers = {
        {"Subscription", Resolver{
            {"messageAdded", SubscriptionResolver{[&pubsub](const ResolverArgs&) {
                return pubsub.AsyncIterator({"NEW_MESSAGE"});
            }}}
        }}
    }
});
```

A `SubscriptionResolver` returns a `SubscriptionEventStream` — typically obtained from `PubSub::AsyncIterator()`.

## Publishing events

To publish an event, you can do it anywhere in your application. Each event is a `ValueResolver` matching the subscription field's return type:

```cpp
pubsub.Publish("NEW_MESSAGE", Resolver{
    {"text", "Hello!"},
    {"author", "Alice"}
});
```

`PubSub` is thread-safe — so you can also publish events from background threads.

## Consuming events

Use `Schema::Subscribe()` to read values from the `SubscriptionHandle`:

```cpp
auto handle = schema.Subscribe({
    .query = "subscription { messageAdded { text author } }"
});

while (auto result = handle.Next()) {
    if (result->errors)
        for (const auto& e : *result->errors)
            std::cerr << e.message << std::endl;
    else
        std::cout << result->data.value().dump(2) << std::endl;
}
```

Calling `Next()` blocks until the next event arrives and returns `std::optional<GraphQLResponse>`. Call `Cancel()` to close the stream and unblock any pending `Next()`:

```cpp
handle.Cancel();
```

## Subscription arguments

Subscription fields accept arguments, useful for topic filtering:

```graphql
type Subscription {
    reviewAdded(bookId: ID): Review!
}
```

```cpp
{"reviewAdded", SubscriptionResolver{[&pubsub](const ResolverArgs& args) {
    if (args.Args().contains("bookId") && !args.Args()["bookId"].is_null()) {
        auto bookId = args.Args()["bookId"].get<std::string>();
        return pubsub.AsyncIterator({"REVIEW_ADDED_" + bookId});
    }
    return pubsub.AsyncIterator({"REVIEW_ADDED"});
}}}
```

## Custom event sources

For event sources beyond `PubSub`, construct a `SubscriptionEventStream` directly:

```cpp
SubscriptionEventStream stream(
    [&queue]() -> ValueResolver { return queue.pop(); },  // next (blocks)
    [&queue]() { queue.shutdown(); }                      // close
);
```

## IsSubscription helper

Use `IsSubscription()` to route a query without parsing the full document:

```cpp
if (IsSubscription(query)) {
    auto handle = schema.Subscribe({.query = query});
    // ...
} else {
    auto result = schema.Resolve({.query = query}).get();
    // ...
}
```

## Error handling

Errors during event execution produce a standard error payload for that event without terminating the stream. The stream remains open and continues delivering subsequent events.
