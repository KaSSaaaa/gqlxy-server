#include <gqlxy/server/internal/channel.h>
#include <gqlxy/server/pubsub.h>
#include <mutex>
#include <unordered_map>
#include <vector>

using namespace std;
using namespace gqlxy;

namespace gqlxy::internal {

class PubSub {
  public:
    void Publish(const string& topic, const ValueResolver& payload) {
        unique_lock lock(_mutex);
        erase_if(_topics[topic], [&](const auto& weak) {
            if (auto channel = weak.lock()) {
                channel->Push(payload);
                return false;
            }
            return true;
        });
    }

    SubscriptionEventStream AsyncIterator(initializer_list<string> topics) {
        auto channel = AddChannel(topics);
        return {
            [channel]() -> ValueResolver { return channel->Next().value_or(monostate {}); },
            [channel]() { channel->Close(); }
        };
    }

  private:
    mutex _mutex;
    unordered_map<string, vector<weak_ptr<Channel<ValueResolver>>>> _topics;

    shared_ptr<Channel<ValueResolver>> AddChannel(const initializer_list<string>& topics) {
        auto channel = make_shared<Channel<ValueResolver>>();
        unique_lock lock(_mutex);
        for (const auto& topic : topics)
            _topics[topic].push_back(channel);
        return channel;
    }
};

}
PubSub::PubSub() : _instance(make_shared<internal::PubSub>()) {}

void PubSub::Publish(const string& topic, const ValueResolver& payload) {
    _instance->Publish(topic, payload);
}

SubscriptionEventStream PubSub::AsyncIterator(initializer_list<string> topics) {
    return _instance->AsyncIterator(topics);
}
