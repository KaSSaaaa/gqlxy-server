#include <gqlxy/core/results.h>
#include <gqlxy/server/definitions/schema_definition.h>
#include <gqlxy/server/internal/mcp/create_mcp_tools.h>
#include <gqlxy/server/internal/mcp/mcp_tool_registry.h>
#include <gqlxy/server/internal/peg/parser/schema_parser.h>
#include <gqlxy/server/mcp/create_mcp_registry.h>
#include <gqlxy/server/mcp/mcp_policy.h>
#include <gqlxy/server/pubsub.h>
#include <gqlxy/server/schema.h>
#include <gtest/gtest.h>
#include <chrono>

using namespace std;
using namespace std::chrono;
using namespace gqlxy;
using namespace gqlxy::internal;
using namespace gqlxy::utils;
using namespace nlohmann;

static Schema MakeSchema(const string& sdl) {
    return Schema({.typeDefs = sdl, .resolvers = {}});
}

TEST(McpExtract, DisabledPolicyReturnsNoTools) {
    auto schema = MakeSchema(R"(
        type Query {
            hello: String
        }
    )");
    auto tools = CreateMcpTools(schema, DefaultMcpPolicy::Disabled);
    EXPECT_TRUE(tools.empty());
}

TEST(McpExtract, EnabledPolicyExposesAllQueryFields) {
    auto schema = MakeSchema(R"(
        type Query {
            hello: String
            world: Int
        }
    )");
    auto tools = CreateMcpTools(schema, DefaultMcpPolicy::Enabled);
    ASSERT_EQ(tools.size(), 2u);

    auto names = vector{tools[0].name, tools[1].name};
    EXPECT_TRUE(ranges::find(names, "Query_hello") != names.end());
    EXPECT_TRUE(ranges::find(names, "Query_world") != names.end());
}

TEST(McpExtract, EnabledPolicyExposesQueryAndMutationFields) {
    auto schema = MakeSchema(R"(
        type Query {
            ping: String
        }

        type Mutation {
            doThing: Boolean
        }
    )");
    auto tools = CreateMcpTools(schema, DefaultMcpPolicy::Enabled);
    ASSERT_EQ(tools.size(), 2u);
    auto names = vector{tools[0].name, tools[1].name};
    EXPECT_TRUE(ranges::find(names, "Query_ping") != names.end());
    EXPECT_TRUE(ranges::find(names, "Mutation_doThing") != names.end());
}

TEST(McpExtract, ToolIncludesDescriptionFromSchema) {
    auto schema = MakeSchema(R"(
        type Query {
            "Returns a greeting"
            hello: String
        }
    )");
    auto tools = CreateMcpTools(schema, DefaultMcpPolicy::Enabled);
    ASSERT_EQ(tools.size(), 1u);
    ASSERT_TRUE(tools[0].description.has_value());
    EXPECT_EQ(*tools[0].description, "Returns a greeting");
}

TEST(McpExtract, ToolArgsReflectFieldArguments) {
    auto schema = MakeSchema(R"(
        type Query {
            greet(name: String!, times: Int): String
        }
    )");
    auto tools = CreateMcpTools(schema, DefaultMcpPolicy::Enabled);
    ASSERT_EQ(tools.size(), 1u);
    ASSERT_EQ(tools[0].args.size(), 2u);

    EXPECT_EQ(tools[0].args[0].name, "name");
    EXPECT_EQ(tools[0].args[0].jsonSchemaType, "string");
    EXPECT_TRUE(tools[0].args[0].required);

    EXPECT_EQ(tools[0].args[1].name, "times");
    EXPECT_EQ(tools[0].args[1].jsonSchemaType, "integer");
    EXPECT_FALSE(tools[0].args[1].required);
}

TEST(McpExtract, EnabledPolicyHidesFieldWithHideMcpDirective) {
    auto schema = MakeSchema(R"(
        directive @hideMcp on FIELD_DEFINITION

        type Query {
            visible: String
            secret: String @hideMcp
        }
    )");
    auto tools = CreateMcpTools(schema, DefaultMcpPolicy::Enabled);
    ASSERT_EQ(tools.size(), 1u);
    EXPECT_EQ(tools[0].name, "Query_visible");
}

TEST(McpExtract, HiddenPolicyExposesFieldWithAllowMcpDirective) {
    auto schema = MakeSchema(R"(
        directive @allowMcp on FIELD_DEFINITION

        type Query {
            hidden: String
            exposed: String @allowMcp
        }
    )");
    auto tools = CreateMcpTools(schema, DefaultMcpPolicy::Hidden);
    ASSERT_EQ(tools.size(), 1u);
    EXPECT_EQ(tools[0].name, "Query_exposed");
}

TEST(McpExtract, AllowMcpNameArgOverridesDefaultToolName) {
    auto schema = MakeSchema(R"(
        directive @allowMcp(name: String, description: String) on FIELD_DEFINITION

        type Query {
            internalField: String @allowMcp(name: "myTool")
        }
    )");
    auto tools = CreateMcpTools(schema, DefaultMcpPolicy::Hidden);
    ASSERT_EQ(tools.size(), 1u);
    EXPECT_EQ(tools[0].name, "myTool");
}

TEST(McpExtract, AllowMcpDescriptionArgOverridesSchemaDescription) {
    auto schema = MakeSchema(R"(
        directive @allowMcp(name: String, description: String) on FIELD_DEFINITION

        type Query {
            "SDL description"
            myField: String @allowMcp(description: "Override description")
        }
    )");
    auto tools = CreateMcpTools(schema, DefaultMcpPolicy::Enabled);
    ASSERT_EQ(tools.size(), 1u);
    ASSERT_TRUE(tools[0].description.has_value());
    EXPECT_EQ(*tools[0].description, "Override description");
}

TEST(McpExtract, AllowMcpWithBothArgsOverridesBoth) {
    auto schema = MakeSchema(R"(
        directive @allowMcp(name: String, description: String) on FIELD_DEFINITION

        type Query {
            "SDL description"
            internalField: String @allowMcp(name: "publicTool", description: "Public description")
        }
    )");
    auto tools = CreateMcpTools(schema, DefaultMcpPolicy::Hidden);
    ASSERT_EQ(tools.size(), 1u);
    EXPECT_EQ(tools[0].name, "publicTool");
    ASSERT_TRUE(tools[0].description.has_value());
    EXPECT_EQ(*tools[0].description, "Public description");
}

TEST(McpExtract, InputSchemaHasCorrectShape) {
    auto schema = MakeSchema(R"(
        type Query {
            add(a: Int!, b: Int!): Int
        }
    )");
    auto tools = CreateMcpTools(schema, DefaultMcpPolicy::Enabled);
    ASSERT_EQ(tools.size(), 1u);

    auto inputSchema = ToJson(tools[0])["inputSchema"];
    EXPECT_EQ(inputSchema["type"], "object");
    EXPECT_TRUE(inputSchema["properties"].contains("a"));
    EXPECT_EQ(inputSchema["properties"]["a"]["type"], "integer");
    EXPECT_TRUE(inputSchema["required"].is_array());
    ASSERT_EQ(inputSchema["required"].size(), 2u);
}

TEST(McpExtract, ScalarReturnTypeHasNoFragmentsArg) {
    auto schema = MakeSchema(R"(
        type Query {
            hello: String
        }
    )");
    auto tools = CreateMcpTools(schema, DefaultMcpPolicy::Enabled);
    ASSERT_EQ(tools.size(), 1u);
    auto argNames = tools[0].args | views::transform([](const auto& a) { return a.name; });
    EXPECT_TRUE(ranges::find(argNames, "fragments") == argNames.end());
}

TEST(McpExtract, ObjectReturnTypeHasFragmentsArg) {
    auto schema = MakeSchema(R"(
        type User { name: String }
        type Query {
            user: User
        }
    )");
    auto tools = CreateMcpTools(schema, DefaultMcpPolicy::Enabled);
    ASSERT_EQ(tools.size(), 1u);
    auto argNames = to_vector(tools[0].args | views::transform([](const auto& a) { return a.name; }));
    EXPECT_TRUE(ranges::find(argNames, "fragments") != argNames.end());
    auto fragArg = *ranges::find_if(tools[0].args, [](const auto& a) { return a.name == "fragments"; });
    EXPECT_FALSE(fragArg.required);
    EXPECT_EQ(fragArg.jsonSchemaType, "array");
}

static Schema MakeUserSchema() {
    return Schema({
        .typeDefs = R"(
            type User { name: String email: String }
            type Query { me: User }
        )",
        .resolvers = {
            {"Query", Resolver{
                {"me", Resolver{
                    {"name", "Alice"},
                    {"email", "alice@example.com"}
                }}
            }}
        },
    });
}

TEST(McpExtract, ToolHandlerWithFragmentsQueriesRequestedFields) {
    auto schema = MakeUserSchema();
    auto tools = CreateMcpTools(schema, DefaultMcpPolicy::Enabled);
    ASSERT_EQ(tools.size(), 1u);

    auto result = tools[0].handler({{"fragments", json::array({"fragment UserFields on User { email }"})}}).Next();
    ASSERT_TRUE(result.has_value());
    auto data = Serialize(*result);
    ASSERT_TRUE(data.contains("data"));
    ASSERT_TRUE(data["data"].contains("me"));
    EXPECT_TRUE(data["data"]["me"].contains("email"));
    EXPECT_FALSE(data["data"]["me"].contains("name"));
}

TEST(McpExtract, ToolHandlerWithoutFragmentsUsesDefaultScalarFields) {
    auto schema = MakeUserSchema();
    auto tools = CreateMcpTools(schema, DefaultMcpPolicy::Enabled);
    ASSERT_EQ(tools.size(), 1u);

    auto result = tools[0].handler({}).Next();
    ASSERT_TRUE(result.has_value());
    auto data = Serialize(*result);
    ASSERT_TRUE(data.contains("data"));
    ASSERT_TRUE(data["data"].contains("me"));
    EXPECT_TRUE(data["data"]["me"].contains("name"));
    EXPECT_TRUE(data["data"]["me"].contains("email"));
}

TEST(McpExtract, SchemaBuildsNonEmptyRegistryWhenEnabled) {
    Schema schema({
        .typeDefs = R"(
            type Query {
                hello: String
            }
        )",
        .resolvers = {},
    });
    auto registry = mcp::CreateMcpRegistry(schema, DefaultMcpPolicy::Enabled);
    EXPECT_FALSE(registry->IsEmpty());
}

TEST(McpExtract, SchemaBuildsEmptyRegistryWhenDisabled) {
    Schema schema({
        .typeDefs = R"(
            type Query {
                hello: String
            }
        )",
        .resolvers = {},
    });
    auto registry = mcp::CreateMcpRegistry(schema, DefaultMcpPolicy::Disabled);
    EXPECT_TRUE(registry->IsEmpty());
}

TEST(McpExtract, SubscriptionFieldsExposedAsTools) {
    auto schema = MakeSchema(R"(
        type Query { hello: String }
        type Subscription { onHello: String }
    )");
    auto tools = CreateMcpTools(schema, DefaultMcpPolicy::Enabled);
    auto names = tools | views::transform([](const auto& t) { return t.name; });
    EXPECT_TRUE(ranges::find(names, "Subscription_onHello") != names.end());
}

TEST(McpExtract, SubscriptionToolHasSubscriptionHandler) {
    auto schema = MakeSchema(R"(
        type Query { hello: String }
        type Subscription { onHello: String }
    )");
    auto tools = CreateMcpTools(schema, DefaultMcpPolicy::Enabled);
    auto it = ranges::find_if(tools, [](const auto& t) { return t.name == "Subscription_onHello"; });
    ASSERT_NE(it, tools.end());
    EXPECT_TRUE(it->handler != nullptr);
}

TEST(McpExtract, SubscriptionToolHandlerReceivesFirstEvent) {
    gqlxy::PubSub pubsub;
    Schema schema({
        .typeDefs = R"(
            type Query { hello: String }
            type Subscription { onHello: String }
        )",
        .resolvers = {
            {"Subscription", Resolver{
                {"onHello", SubscriptionResolver{[&pubsub](const ResolverArgs&) {
                    return pubsub.AsyncIterator({"onHello"});
                }}}
            }}
        },
    });
    auto tools = CreateMcpTools(schema, DefaultMcpPolicy::Enabled);
    auto it = ranges::find_if(tools, [](const auto& t) { return t.name == "Subscription_onHello"; });
    ASSERT_NE(it, tools.end());

    auto publisher = async([&]() {
        this_thread::sleep_for(milliseconds(10));
        pubsub.Publish("onHello", "world");
    });
    auto result = it->handler({}).Next();
    publisher.get();
    ASSERT_TRUE(result.has_value());
    EXPECT_FALSE(result->errors.has_value());
    ASSERT_TRUE(result->data.has_value());
    EXPECT_EQ((*result->data)["onHello"], "world");
}

TEST(McpExtract, SubscriptionToolSubscriptionHandlerStreamsEvents) {
    gqlxy::PubSub pubsub;
    Schema schema({
        .typeDefs = R"(
            type Query { hello: String }
            type Subscription { onHello: String }
        )",
        .resolvers = {
            {"Subscription", Resolver{
                {"onHello", SubscriptionResolver{[&pubsub](const ResolverArgs&) {
                    return pubsub.AsyncIterator({"onHello"});
                }}}
            }}
        },
    });
    auto tools = CreateMcpTools(schema, DefaultMcpPolicy::Enabled);
    auto it = ranges::find_if(tools, [](const auto& t) { return t.name == "Subscription_onHello"; });
    ASSERT_NE(it, tools.end());

    auto handle = it->handler({});
    pubsub.Publish("onHello", "first");
    pubsub.Publish("onHello", "second");
    auto r1 = handle.Next();
    auto r2 = handle.Next();

    ASSERT_TRUE(r1.has_value());
    EXPECT_EQ(Serialize(*r1)["data"]["onHello"], "first");
    ASSERT_TRUE(r2.has_value());
    EXPECT_EQ(Serialize(*r2)["data"]["onHello"], "second");
}
