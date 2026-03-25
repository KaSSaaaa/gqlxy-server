#include <gqlxy/subscription.h>

#include <gqlxy/internal/peg/parser/query/ParseDocument.h>
#include <gqlxy/internal/ast/Selection.h>
#include <gqlxy/resolvers.h>

using namespace std;
using namespace gqlxy;

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

SubscriptionHandle SubscriptionHandle::SingleShot(const ResolveResult& result) {
    auto fired = make_shared<bool>(false);
    return SubscriptionHandle {
        [fired, result]() -> optional<ResolveResult> {
            if (*fired) return nullopt;
            *fired = true;
            return result;
        },
        [] {}};
}

namespace gqlxy {

bool IsSubscription(const string& query) {
    return ranges::any_of(internal::ParseDocument(query).operations, [](const internal::OperationDefinition& operation) {
        return operation.type._value == internal::OperationType::SUBSCRIPTION;
    });
}

}
