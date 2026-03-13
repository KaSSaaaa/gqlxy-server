#include <ariane/schema.h>
#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <ariane/internal/utils/expect.h>
#include <format>
#include <fstream>
#include <sstream>

using namespace std;
using namespace ariane::graphql;
using json = nlohmann::json;

static string readFile(const char* path) {
    ifstream f(path);
    internal::expect(!!f, format("Cannot open file: {}", path));
    ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

class IntrospectionEndToEndTest : public testing::Test {
protected:
    void SetUp() override {
        Schema schema({
            .typeDefs  = readFile(SCHEMA_TODAY_PATH),
            .resolvers = {
                {"Query", Resolver{
                    {"__typename", "Query"}
                }}
            }
        });

        auto result = schema.Resolve({
            .query = readFile(INTROSPECTION_QUERY_PATH)
        }).get();
        ASSERT_FALSE(result.errors.has_value()) << "Unexpected errors: " << result.errors.value()[0].message;

        _data     = json::parse(result.data.value());
        _schema   = _data["__schema"];
        _expected = json::parse(readFile(INTROSPECTION_RESULT_PATH))["__schema"];
    }

    json _data;
    json _schema;
    json _expected;
};

TEST_F(IntrospectionEndToEndTest, MatchesExpectedResult) {
    EXPECT_EQ(_schema, _expected);
}

TEST_F(IntrospectionEndToEndTest, ReturnsCorrectRootTypes) {
    EXPECT_EQ(_schema["queryType"]["name"], "Query");
    EXPECT_EQ(_schema["mutationType"]["name"], "Mutation");
    EXPECT_EQ(_schema["subscriptionType"]["name"], "Subscription");
}

TEST_F(IntrospectionEndToEndTest, ReturnsExpectedTypeCount) {
    EXPECT_EQ(_schema["types"].size(), _expected["types"].size());
}

TEST_F(IntrospectionEndToEndTest, TypesIncludeUserDefinedAndBuiltIn) {
    vector<string> expected = {"Query", "Mutation", "Subscription", "Task",
                               "Appointment", "Folder", "TaskState", "Node",
                               "__Schema", "__Type", "__Field"};

    auto typeNames = _schema["types"]
        | views::transform([](const json& t) { return t["name"].get<string>(); });
    set<string> names(typeNames.begin(), typeNames.end());

    for (const auto& name : expected) {
        EXPECT_TRUE(names.contains(name)) << "Missing type: " << name;
    }
}

TEST_F(IntrospectionEndToEndTest, EnumTypeHasCorrectValues) {
    auto it = ranges::find_if(_schema["types"], [](const json& t) {
        return t["name"] == "TaskState";
    });
    ASSERT_NE(it, _schema["types"].end());

    auto enumValues = *it;
    ASSERT_TRUE(enumValues.contains("enumValues"));

    vector<string> valueNames;
    for (const auto& v : enumValues["enumValues"]) {
        valueNames.push_back(v["name"]);
    }
    EXPECT_EQ(valueNames, vector<string>({"Unassigned", "New", "Started", "Complete"}));
}

TEST_F(IntrospectionEndToEndTest, UnionTypeHasCorrectPossibleTypes) {
    auto it = ranges::find_if(_schema["types"], [](const json& t) {
        return t["name"] == "UnionType";
    });
    ASSERT_NE(it, _schema["types"].end());

    auto possibleTypes = (*it)["possibleTypes"];
    ASSERT_EQ(possibleTypes.size(), 3);

    set<string> names;
    for (const auto& t : possibleTypes) names.insert(t["name"]);
    EXPECT_TRUE(names.contains("Appointment"));
    EXPECT_TRUE(names.contains("Task"));
    EXPECT_TRUE(names.contains("Folder"));
}

TEST_F(IntrospectionEndToEndTest, ObjectTypeImplementsInterface) {
    auto it = ranges::find_if(_schema["types"], [](const json& t) {
        return t["name"] == "Appointment";
    });
    ASSERT_NE(it, _schema["types"].end());

    auto interfaces = (*it)["interfaces"];
    ASSERT_EQ(interfaces.size(), 1);
    EXPECT_EQ(interfaces[0]["name"], "Node");
}

TEST_F(IntrospectionEndToEndTest, DirectivesIncludeBuiltInAndCustom) {
    auto directiveNames = _schema["directives"]
        | views::transform([](const json& d) { return d["name"].get<string>(); });
    set<string> names(directiveNames.begin(), directiveNames.end());

    for (const auto& name : {"deprecated", "include", "skip", "id", "queryTag"}) {
        EXPECT_TRUE(names.contains(name)) << "Missing directive: " << name;
    }
}
