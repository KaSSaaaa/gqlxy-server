#include <ariane/optional.h>
#include <gtest/gtest.h>
#include <memory>
#include <string>

using namespace ariane::graphql;

TEST(Optional, AndThenWithValue) {
    std::optional<int> opt = 42;
    auto result = and_then(opt, [](int x) { return std::optional<int>(x * 2); });
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, 84);
}

TEST(Optional, AndThenWithEmpty) {
    std::optional<int> opt;
    auto result = and_then(opt, [](int x) { return std::optional<int>(x * 2); });
    EXPECT_FALSE(result.has_value());
}

TEST(Optional, AndThenChaining) {
    std::optional<int> opt = 10;
    auto result = and_then(and_then(opt, [](int x) { return std::optional<int>(x + 5); }),
                           [](int x) { return std::optional<int>(x * 2); });
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, 30);
}

TEST(Optional, AndThenChainingWithEmpty) {
    std::optional<int> opt = 10;
    auto result = and_then(and_then(opt, [](int) { return std::optional<int>(); }),
                           [](int x) { return std::optional<int>(x * 2); });
    EXPECT_FALSE(result.has_value());
}

TEST(Optional, AndThenWithRvalue) {
    auto result = and_then(std::optional<int>(42), [](int x) { return std::optional<int>(x * 2); });
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, 84);
}

TEST(Optional, AndThenWithMoveOnlyType) {
    std::optional<std::unique_ptr<int>> opt = std::make_unique<int>(42);
    auto result = and_then(opt, [](const std::unique_ptr<int>& ptr) { return std::optional<int>(*ptr * 2); });
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, 84);
}

TEST(Optional, AndThenReturnsEmpty) {
    std::optional<int> opt = 42;
    auto result = and_then(opt, [](int) { return std::optional<int>(); });
    EXPECT_FALSE(result.has_value());
}

TEST(Optional, AndThenWithString) {
    std::optional<std::string> opt = "hello";
    auto result = and_then(opt, [](const std::string& s) { return std::optional<size_t>(s.length()); });
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, 5);
}

TEST(Optional, AndThenWithComplexType) {
    struct Person {
        std::string name;
        int age;
    };
    std::optional<Person> opt = Person{"Alice", 30};
    auto result = and_then(opt, [](const Person& p) { return std::optional<std::string>(p.name); });
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, "Alice");
}

TEST(Optional, AndThenWithModifiableValue) {
    std::optional<int> opt = 10;
    auto result = and_then(opt, [](int x) {
        x += 5;
        return std::optional<int>(x);
    });
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, 15);
    EXPECT_EQ(*opt, 10);
}

TEST(Optional, AndThenWithDifferentTypes) {
    std::optional<int> opt = 42;
    auto result = and_then(opt, [](int x) { return std::optional<std::string>(std::to_string(x)); });
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, "42");
}
