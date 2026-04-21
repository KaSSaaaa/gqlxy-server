#pragma once

#include <gqlxy/results.h>
#include <functional>
#include <optional>

namespace gqlxy {
struct ValueResolver;

class SubscriptionEventStream {
public:
    SubscriptionEventStream() = default;
    SubscriptionEventStream(std::function<ValueResolver()> next, std::function<void()> close);

    ValueResolver Next();
    void Close();
    bool Valid() const;

private:
    std::function<ValueResolver()> _next;
    std::function<void()> _close;
};

class SubscriptionHandle {
public:
    SubscriptionHandle(std::function<std::optional<GraphQLResponse>()> next,
                       std::function<void()> cancel);

    SubscriptionHandle(const SubscriptionHandle&) = delete;
    SubscriptionHandle& operator=(const SubscriptionHandle&) = delete;
    SubscriptionHandle(SubscriptionHandle&&) noexcept = default;
    SubscriptionHandle& operator=(SubscriptionHandle&&) noexcept = default;

    std::optional<GraphQLResponse> Next();
    void Cancel();

    static SubscriptionHandle SingleShot(const GraphQLResponse& result);

private:
    std::function<std::optional<GraphQLResponse>()> _next;
    std::function<void()> _cancel;
};

bool IsSubscription(const std::string& query);

}
