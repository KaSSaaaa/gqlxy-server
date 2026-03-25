# Subscriptions

Ariane supports GraphQL subscriptions via a built-in `PubSub` system. Subscriptions establish a long-lived event source that maps each published event to a GraphQL response.

## Defining a subscription

Add a `Subscription` type to your SDL and a `SubscriptionResolver` to your resolver map:

```cpp
#include <ariane/pubsub.h>
#include <ariane/schema.h>
#include <ariane/subscription.h>

using namespace ariane::graphql;

PubSub pubsub;

Schema schema({
    .typeDefs = R"(
        type Query { _unused: String }
        type Subscription {
            messageAdded: Message
        }
        type Message { text: String author: String }
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

Publish events from anywhere in your application. Each event is a `ValueResolver` that matches the subscription field's return type:

```cpp
pubsub.Publish("NEW_MESSAGE", Resolver{
    {"text", "Hello!"},
    {"author", "Alice"}
});
```

## Subscribing

Use `Schema::Subscribe()` instead of `Schema::Resolve()`. It returns a `SubscriptionHandle`:

```cpp
auto handle = schema.Subscribe({
    .query = "subscription { messageAdded { text author } }"
});
```

### Consuming events

Call `handle.Next()` in a loop. It blocks until the next event arrives and returns `std::optional<ResolveResult>`:

```cpp
while (auto result = handle.Next()) {
    if (!result.has_value()) break;

    if (result->errors.has_value()) {
        for (const auto& e : *result->errors)
            std::cerr << "Error: " << e.message << std::endl;
        continue;
    }

    std::cout << result->data.value().dump(2) << std::endl;
}
```

Each event goes through the full field-execution pipeline (argument binding, nested resolvers, error handling), producing a standard `ResolveResult`.

### Cancelling

Call `Cancel()` to close the event stream and unblock any pending `Next()` call:

```cpp
handle.Cancel();
```

## Subscription arguments

Subscription fields can accept arguments, allowing clients to filter events:

```cpp
.typeDefs = R"(
    type Subscription {
        reviewAdded(bookId: ID): Review!
    }
)",
.resolvers = {
    {"Subscription", Resolver{
        {"reviewAdded", SubscriptionResolver{[&pubsub](const ResolverArgs& args) {
            if (args.Args().contains("bookId") && !args.Args()["bookId"].is_null()) {
                auto bookId = args.Args()["bookId"].get<std::string>();
                return pubsub.AsyncIterator({"REVIEW_ADDED_" + bookId});
            }
            return pubsub.AsyncIterator({"REVIEW_ADDED"});
        }}}
    }}
}
```

## Async publishing

Publish events from background threads. `PubSub` is thread-safe:

```cpp
auto publisher = std::async(std::launch::async, [&pubsub] {
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        pubsub.Publish("TICK", currentDatetime());
    }
});
```

## Error handling

Errors during event execution produce a standard error payload for that event without terminating the stream:

```json
{
  "data": null,
  "errors": [
    {
      "message": "resolver failed",
      "path": [
        "messageAdded"
      ]
    }
  ]
}
```

The stream remains open and continues delivering subsequent events.

## Single root field enforcement

The GraphQL spec forbids subscription documents with more than one root field (excluding `__typename`). Ariane rejects such documents with a validation error before execution.

## SubscriptionEventStream

If you need custom event sources beyond `PubSub`, construct a `SubscriptionEventStream` directly:

```cpp
SubscriptionEventStream stream(
    // next — called to get each event (blocks until available)
    [&queue]() -> ValueResolver { return queue.pop(); },
    // close — called when the subscription is cancelled
    [&queue]() { queue.shutdown(); }
);
```

## Helper: IsSubscription

Use `IsSubscription()` to check if a query string is a subscription operation without parsing the full document:

```cpp
#include <ariane/subscription.h>

if (IsSubscription(query)) {
    auto handle = schema.Subscribe({.query = query});
    // ...
} else {
    auto result = schema.Resolve({.query = query}).get();
    // ...
}
```
