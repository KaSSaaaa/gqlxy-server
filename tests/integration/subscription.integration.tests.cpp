#include <gqlxy/server/pubsub.h>
#include <gqlxy/server/schema.h>
#include <gqlxy/server/subscription.h>
#include <gtest/gtest.h>
#include <future>
#include <nlohmann/json.hpp>
#include <thread>

using namespace std;
using namespace gqlxy;
using json = nlohmann::json;

class SubscriptionIntegrationTest : public testing::Test {
protected:
    const string typeDefs = R"(
        type Query { _unused: String }
        type Subscription { counter: Int }
    )";

    Schema CreateSchema(PubSub& pubsub) {
        return Schema({
            .typeDefs = typeDefs,
            .resolvers = {
                {"Subscription", Resolver{
                    {"counter", SubscriptionResolver{[&pubsub](const ResolverArgs&) {
                        return pubsub.AsyncIterator({"COUNTER"});
                    }}}
                }}
            }
        });
    }
};

TEST_F(SubscriptionIntegrationTest, CancelUnblocksBlockingNext) {
    PubSub pubsub;
    auto schema = CreateSchema(pubsub);

    auto handle = schema.Subscribe({.query = "subscription { counter }"});

    auto fut = async(launch::async, [&handle] {
        return handle.Next();
    });

    this_thread::sleep_for(chrono::milliseconds(50));
    handle.Cancel();

    auto result = fut.get();
    EXPECT_FALSE(result.has_value());
}

TEST_F(SubscriptionIntegrationTest, AsyncPublishFromAnotherThread) {
    PubSub pubsub;
    auto schema = CreateSchema(pubsub);

    auto handle = schema.Subscribe({.query = "subscription { counter }"});

    auto publisher = async(launch::async, [&pubsub] {
        this_thread::sleep_for(chrono::milliseconds(10));
        pubsub.Publish("COUNTER", 99);
    });

    auto result = handle.Next();
    publisher.get();
    handle.Cancel();

    ASSERT_TRUE(result.has_value());
    EXPECT_FALSE(result->errors.has_value());
    EXPECT_EQ(result->data.value()["counter"], 99);
}
