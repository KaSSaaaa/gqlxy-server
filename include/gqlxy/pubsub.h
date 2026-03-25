#pragma once

#include "resolvers.h"
#include <gqlxy/subscription.h>
#include <initializer_list>
#include <string>

namespace gqlxy::internal {
class PubSub;
}

namespace gqlxy {

class PubSub {
public:
    PubSub();
    ~PubSub() = default;

    void Publish(const std::string& topic, const ValueResolver& payload);
    SubscriptionEventStream AsyncIterator(std::initializer_list<std::string> topics);

private:
    std::shared_ptr<internal::PubSub> _instance;
};

}
