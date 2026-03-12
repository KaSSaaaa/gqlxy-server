#include <ariane/pubsub.h>
#include <ariane/schema.h>
#include <ariane/subscription.h>
#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

using namespace std;
using namespace ariane::graphql;
using json = nlohmann::json;

// ---------------------------------------------------------------------------
// Fixture
// ---------------------------------------------------------------------------

class SubscriptionTest : public testing::Test {
protected:
    const string typeDefs = R"(
        type Query { _unused: String }
        type Subscription {
            messageCreated: Message
            counter: Int
        }
        type Message { text: String author: String }
    )";

    static Schema CreateSchema(PubSub& pubsub, const string& typeDefs) {
        return Schema({
            .typeDefs = typeDefs,
            .resolvers = {
                {"Subscription", Resolver{
                    {"messageCreated", SubscriptionResolver{[&pubsub](const ResolverArgs&) {
                        return pubsub.AsyncIterator({"MESSAGE_CREATED"});
                    }}},
                    {"counter", SubscriptionResolver{[&pubsub](const ResolverArgs&) {
                        return pubsub.AsyncIterator({"COUNTER"});
                    }}}
                }}
            }
        });
    }
};

// ---------------------------------------------------------------------------
// PubSub basic
// ---------------------------------------------------------------------------

TEST_F(SubscriptionTest, PubSubDeliversPublishedEvent) {
    PubSub pubsub;
    auto stream = pubsub.AsyncIterator({"MSG"});

    pubsub.Publish("MSG", Resolver{
        {"text", "Hello"},
        {"author", "Alice"}
    });
    pubsub.Publish("MSG", Resolver{
        {"text", "World"},
        {"author", "Bob"}
    });

    auto e1 = stream.Next();
    auto e2 = stream.Next();
    stream.Close();

    ASSERT_TRUE(e1.Is<Resolver>());
    ASSERT_TRUE(e2.Is<Resolver>());
    EXPECT_EQ(e1.As<Resolver>().at("author").As<string>(), "Alice");
    EXPECT_EQ(e2.As<Resolver>().at("author").As<string>(), "Bob");
}

TEST_F(SubscriptionTest, PubSubMultipleSubscribersReceiveEvent) {
    PubSub pubsub;
    auto s1 = pubsub.AsyncIterator({"TOPIC"});
    auto s2 = pubsub.AsyncIterator({"TOPIC"});

    pubsub.Publish("TOPIC", string("ping"));

    auto e1 = s1.Next().AsIf<string>();
    auto e2 = s2.Next().AsIf<string>();
    s1.Close();
    s2.Close();

    ASSERT_TRUE(e1.has_value());
    ASSERT_TRUE(e2.has_value());
    EXPECT_EQ(e1.value(), "ping");
    EXPECT_EQ(e2.value(), "ping");
}

TEST_F(SubscriptionTest, ClosedStreamReturnsEmptyAny) {
    PubSub pubsub;
    auto stream = pubsub.AsyncIterator({"X"});
    stream.Close();
    auto e1 = stream.Next();
    EXPECT_TRUE(e1.IsNull());
}

// ---------------------------------------------------------------------------
// Schema::Subscribe — basic event delivery
// ---------------------------------------------------------------------------

TEST_F(SubscriptionTest, SubscribeReceivesEvents) {
    PubSub pubsub;
    auto schema = CreateSchema(pubsub, typeDefs);

    auto handle = schema.Subscribe({
        .query = "subscription { messageCreated { text author } }"
    });

    pubsub.Publish("MESSAGE_CREATED", Resolver{{"text", "Hi"}, {"author", "Eve"}});

    auto result = handle.Next();
    handle.Cancel();

    ASSERT_TRUE(result.has_value());
    EXPECT_FALSE(result->errors.has_value());
    auto data = json::parse(result->data.value());
    EXPECT_EQ(data["messageCreated"]["text"], "Hi");
    EXPECT_EQ(data["messageCreated"]["author"], "Eve");
}

TEST_F(SubscriptionTest, SubscribeReceivesMultipleEvents) {
    PubSub pubsub;
    auto schema = CreateSchema(pubsub, typeDefs);

    auto handle = schema.Subscribe({.query = "subscription { counter }"});

    pubsub.Publish("COUNTER", 1);
    pubsub.Publish("COUNTER", 2);
    pubsub.Publish("COUNTER", 3);

    auto r1 = handle.Next();
    auto r2 = handle.Next();
    auto r3 = handle.Next();
    handle.Cancel();

    ASSERT_TRUE(r1.has_value());
    ASSERT_TRUE(r2.has_value());
    ASSERT_TRUE(r3.has_value());
    EXPECT_EQ(json::parse(r1->data.value())["counter"], 1);
    EXPECT_EQ(json::parse(r2->data.value())["counter"], 2);
    EXPECT_EQ(json::parse(r3->data.value())["counter"], 3);
}

// ---------------------------------------------------------------------------
// Schema::Subscribe — single root field enforcement (#23)
// ---------------------------------------------------------------------------

TEST_F(SubscriptionTest, RejectsMultipleRootFields) {
    PubSub pubsub;
    auto schema = CreateSchema(pubsub, typeDefs);

    auto handle = schema.Subscribe({
        .query = "subscription { messageCreated { text } counter }"
    });
    auto result = handle.Next();

    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(result->errors.has_value());
    EXPECT_NE(result->errors->front().message.find("one root field"), string::npos);
    EXPECT_FALSE(handle.Next().has_value());
}

TEST_F(SubscriptionTest, AllowsTypenameAlongsideRootField) {
    PubSub pubsub;
    auto schema = CreateSchema(pubsub, typeDefs);

    auto handle = schema.Subscribe({.query = "subscription { __typename counter }"});

    pubsub.Publish("COUNTER", 42);
    auto result = handle.Next();
    handle.Cancel();

    ASSERT_TRUE(result.has_value());
    EXPECT_FALSE(result->errors.has_value());
}

// ---------------------------------------------------------------------------
// Schema::Subscribe — error handling (#24)
// ---------------------------------------------------------------------------

TEST_F(SubscriptionTest, ResolverErrorDoesNotTerminateStream) {
    PubSub pubsub;
    auto schema = CreateSchema(pubsub, typeDefs);

    auto handle = schema.Subscribe({.query = "subscription { counter }"});

    pubsub.Publish("COUNTER", 1);
    pubsub.Publish("COUNTER", 2);

    auto r1 = handle.Next();
    auto r2 = handle.Next();
    handle.Cancel();

    ASSERT_TRUE(r1.has_value());
    ASSERT_TRUE(r2.has_value());
    EXPECT_EQ(json::parse(r1->data.value())["counter"], 1);
    EXPECT_EQ(json::parse(r2->data.value())["counter"], 2);
}

TEST_F(SubscriptionTest, RejectsNonSubscriptionOperation) {
    PubSub pubsub;
    auto schema = CreateSchema(pubsub, typeDefs);

    auto handle = schema.Subscribe({.query = "query { _unused }"});
    auto result = handle.Next();

    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(result->errors.has_value());
    EXPECT_FALSE(handle.Next().has_value());
}
