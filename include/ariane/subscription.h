#pragma once

#include <ariane/results.h>
#include <functional>
#include <optional>

namespace ariane::graphql {
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
    SubscriptionHandle(std::function<std::optional<ResolveResult>()> next,
                       std::function<void()> cancel);

    SubscriptionHandle(const SubscriptionHandle&) = delete;
    SubscriptionHandle& operator=(const SubscriptionHandle&) = delete;
    SubscriptionHandle(SubscriptionHandle&&) noexcept = default;
    SubscriptionHandle& operator=(SubscriptionHandle&&) noexcept = default;

    std::optional<ResolveResult> Next();
    void Cancel();

private:
    std::function<std::optional<ResolveResult>()> _next;
    std::function<void()> _cancel;
};

}
