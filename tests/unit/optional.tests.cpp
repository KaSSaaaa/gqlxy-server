#include <ariane/internal/utils/optional.h>
#include <gtest/gtest.h>
#include <memory>
#include <string>

using namespace std;
using namespace ariane::graphql::internal;

TEST(Optional, AndThenWithValue) {
    optional opt = 42;
    auto result = and_then(opt, [](int x) {
        return make_optional(x * 2);
    });
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, 84);
}

TEST(Optional, AndThenWithEmpty) {
    optional<int> opt;
    auto result = and_then(opt, [](int x) {
        return make_optional(x * 2);
    });
    EXPECT_FALSE(result.has_value());
}

TEST(Optional, AndThenChaining) {
    optional opt = 10;
    auto result = and_then(and_then(opt, [](int x) {
        return make_optional(x + 5);
    }), [](int x) {
        return make_optional(x * 2);
    });
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, 30);
}

TEST(Optional, AndThenChainingWithEmpty) {
    optional opt = 10;
    auto result = and_then(and_then(opt, [](int) -> optional<int> {
        return nullopt;
    }), [](int x) {
        return make_optional(x * 2);
    });
    EXPECT_FALSE(result.has_value());
}

TEST(Optional, AndThenWithRvalue) {
    auto result = and_then(make_optional(42), [](int x) {
        return make_optional(x * 2);
    });
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, 84);
}

TEST(Optional, AndThenWithMoveOnlyType) {
    optional opt = make_unique<int>(42);
    auto result = and_then(opt, [](const unique_ptr<int>& ptr) {
        return make_optional(*ptr * 2);
    });
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, 84);
}

TEST(Optional, AndThenReturnsEmpty) {
    optional opt = 42;
    auto result = and_then(opt, [](int) -> optional<int> {
        return nullopt;
    });
    EXPECT_FALSE(result.has_value());
}

TEST(Optional, AndThenWithString) {
    optional<string> opt = "hello";
    auto result = and_then(opt, [](const string& s) {
        return make_optional(s.length());
    });
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, 5);
}

TEST(Optional, AndThenWithComplexType) {
    struct Person {
        string name;
        int age;
    };
    optional opt = Person{"Alice", 30};
    auto result = and_then(opt, [](const Person& p) {
        return make_optional(p.name);
    });
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, "Alice");
}

TEST(Optional, AndThenWithModifiableValue) {
    optional opt = 10;
    auto result = and_then(opt, [](int x) {
        x += 5;
        return make_optional(x);
    });
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, 15);
    EXPECT_EQ(*opt, 10);
}

TEST(Optional, AndThenWithDifferentTypes) {
    optional opt = 42;
    auto result = and_then(opt, [](int x) {
        return make_optional(to_string(x));
    });
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, "42");
}

TEST(Optional, OrElseWithValue) {
    optional opt = 42;
    auto result = or_else(opt, []() { return make_optional(99); });
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, 42);
}

TEST(Optional, OrElseWithEmpty) {
    optional<int> opt;
    auto result = or_else(opt, []() { return make_optional(99); });
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, 99);
}

TEST(Optional, OrElseWithEmptyReturnsNullopt) {
    optional<int> opt;
    auto result = or_else(opt, []() -> optional<int> { return nullopt; });
    EXPECT_FALSE(result.has_value());
}

TEST(Optional, OrElseChaining) {
    optional<int> opt;
    auto result = or_else(or_else(opt, []() -> optional<int> { return nullopt; }),
                          []() { return make_optional(7); });
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, 7);
}

TEST(Optional, OrElseCallbackNotCalledWhenValuePresent) {
    optional opt = 1;
    int calls = 0;
    or_else(opt, [&]() -> optional<int> {
        ++calls;
        return make_optional(99);
    });
    EXPECT_EQ(calls, 0);
}

TEST(Optional, OrElseWithString) {
    optional<string> opt;
    auto result = or_else(opt, []() { return make_optional<string>("fallback"); });
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, "fallback");
}
