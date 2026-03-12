#pragma once

#include "resolvers.h"
#include <ariane/subscription.h>
#include <initializer_list>
#include <string>

namespace ariane::graphql::internal {
class PubSub;
}

namespace ariane::graphql {

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
