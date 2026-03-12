#include <ariane/subscription.h>
#include <ariane/resolvers.h>

using namespace std;
using namespace ariane::graphql;

//SubscriptionEventStream

SubscriptionEventStream::SubscriptionEventStream(function<ValueResolver()> next, function<void()> close)
    : _next(std::move(next)), _close(std::move(close)) {}

ValueResolver SubscriptionEventStream::Next() {
    return _next ? _next() : monostate{};
}

void SubscriptionEventStream::Close() {
    if (_close) _close();
}

bool SubscriptionEventStream::Valid() const {
    return static_cast<bool>(_next);
}

//SubscriptionHandle

SubscriptionHandle::SubscriptionHandle(function<optional<ResolveResult>()> next,
                                       function<void()> cancel)
    : _next(std::move(next)), _cancel(std::move(cancel)) {}

optional<ResolveResult> SubscriptionHandle::Next() {
    return _next();
}

void SubscriptionHandle::Cancel() {
    _cancel();
}
