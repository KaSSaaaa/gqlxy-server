#include <ariane/resolvers.h>
#include <gtest/gtest.h>

#include <list>
#include <optional>

using namespace ariane::graphql;

TEST(Resolvers, SupportsBasicTypes) {
    ValueResolver intResolver(42);
    EXPECT_TRUE(std::holds_alternative<int>(intResolver));
    EXPECT_EQ(std::get<int>(intResolver), 42);

    ValueResolver uint64Resolver(uint64_t(42));
    EXPECT_TRUE(std::holds_alternative<uint64_t>(uint64Resolver));
    EXPECT_EQ(std::get<uint64_t>(uint64Resolver), 42);

    ValueResolver stringResolver(std::string("Hello"));
    EXPECT_TRUE(std::holds_alternative<std::string>(stringResolver));
    EXPECT_EQ(std::get<std::string>(stringResolver), "Hello");

    ValueResolver doubleResolver(3.14);
    EXPECT_TRUE(std::holds_alternative<double>(doubleResolver));
    EXPECT_DOUBLE_EQ(std::get<double>(doubleResolver), 3.14);

    ValueResolver floatResolver(3.14f);
    EXPECT_TRUE(std::holds_alternative<float>(floatResolver));
    EXPECT_FLOAT_EQ(std::get<float>(floatResolver), 3.14f);

    ValueResolver boolResolver(true);
    EXPECT_TRUE(std::holds_alternative<bool>(boolResolver));
    EXPECT_EQ(std::get<bool>(boolResolver), true);
}

TEST(Resolvers, SupportsNullValues) {
    ValueResolver nullResolver(std::nullopt);
    EXPECT_TRUE(std::holds_alternative<std::monostate>(nullResolver));

    ValueResolver monostateResolver(std::monostate{});
    EXPECT_TRUE(std::holds_alternative<std::monostate>(monostateResolver));
}

TEST(Resolvers, SupportsOptionalTypes) {
    std::optional<int> presentInt = 42;
    ValueResolver resolver1(presentInt);
    EXPECT_TRUE(std::holds_alternative<int>(resolver1));
    EXPECT_EQ(std::get<int>(resolver1), 42);

    std::optional<std::string> presentString = "Hello";
    ValueResolver resolver2(presentString);
    EXPECT_TRUE(std::holds_alternative<std::string>(resolver2));
    EXPECT_EQ(std::get<std::string>(resolver2), "Hello");

    std::optional<int> absentInt = std::nullopt;
    ValueResolver resolver3(absentInt);
    EXPECT_TRUE(std::holds_alternative<std::monostate>(resolver3));

    std::optional<std::string> absentString = std::nullopt;
    ValueResolver resolver4(absentString);
    EXPECT_TRUE(std::holds_alternative<std::monostate>(resolver4));
}

TEST(Resolvers, SupportsFunctionResolvers) {
    auto funcResolver = []() -> ValueResolver { return ValueResolver(42); };

    ValueResolver resolver(funcResolver);
    EXPECT_TRUE(std::holds_alternative<FunctionResolver>(resolver));
}

TEST(Resolvers, SupportsAsyncFunctionResolvers) {
    auto asyncResolver = []() -> std::future<ValueResolver> {
        return std::async(std::launch::async, []() -> ValueResolver { return ValueResolver(42); });
    };

    ValueResolver resolver(asyncResolver);
    EXPECT_TRUE(std::holds_alternative<AsyncFunctionResolver>(resolver));
}

TEST(Resolvers, SupportsOptionalFunctionForNullables) {
    auto optFunc = []() -> std::optional<ValueResolver> { return std::nullopt; };

    auto wrappedResolver = [optFunc]() -> ValueResolver {
        auto result = optFunc();
        return result.value_or(ValueResolver(std::monostate{}));
    };

    ValueResolver resolver(wrappedResolver);
    EXPECT_TRUE(std::holds_alternative<FunctionResolver>(resolver));

    auto& func = std::get<FunctionResolver>(resolver);
    ValueResolver result = func();
    EXPECT_TRUE(std::holds_alternative<std::monostate>(result));
}

TEST(Resolvers, SupportsNestedResolvers) {
    Resolver nestedResolver = {
         {"id", ValueResolver(123)},
         {"name", ValueResolver(std::string("John"))},
         {"nullable", ValueResolver(std::nullopt)},
    };

    ValueResolver resolver(nestedResolver);
    EXPECT_TRUE(std::holds_alternative<Resolver>(resolver));

    auto& resolverList = std::get<Resolver>(resolver);
    std::unordered_map<std::string, ValueResolver> resolverMap(resolverList.begin(), resolverList.end());
    EXPECT_EQ(resolverMap.size(), 3);

    EXPECT_TRUE(std::holds_alternative<std::monostate>(resolverMap.at("nullable")));
}

TEST(Resolvers, SupportsVectorOfResolvers) {
    std::vector<ValueResolver> vec;
    vec.push_back(ValueResolver(1));
    vec.push_back(ValueResolver(2));
    vec.push_back(ValueResolver(3));

    ValueResolver resolver(vec);
    EXPECT_TRUE(std::holds_alternative<std::vector<ValueResolver>>(resolver));

    auto& resolverVec = std::get<std::vector<ValueResolver>>(resolver);
    EXPECT_EQ(resolverVec.size(), 3);
    EXPECT_TRUE(std::holds_alternative<int>(resolverVec[0]));
    EXPECT_EQ(std::get<int>(resolverVec[0]), 1);
    EXPECT_EQ(std::get<int>(resolverVec[1]), 2);
    EXPECT_EQ(std::get<int>(resolverVec[2]), 3);
}

TEST(Resolvers, SupportsInitializerListOfResolvers) {
    ValueResolver resolver({ValueResolver(1), ValueResolver(2), ValueResolver(3)});
    EXPECT_TRUE(std::holds_alternative<std::vector<ValueResolver>>(resolver));

    auto& resolverVec = std::get<std::vector<ValueResolver>>(resolver);
    EXPECT_EQ(resolverVec.size(), 3);
    EXPECT_EQ(std::get<int>(resolverVec[0]), 1);
    EXPECT_EQ(std::get<int>(resolverVec[1]), 2);
    EXPECT_EQ(std::get<int>(resolverVec[2]), 3);
}

TEST(Resolvers, SupportsListOfResolvers) {
    std::list<ValueResolver> list;
    list.push_back(ValueResolver(std::string("first")));
    list.push_back(ValueResolver(std::string("second")));
    list.push_back(ValueResolver(std::string("third")));

    ValueResolver resolver(list);
    EXPECT_TRUE(std::holds_alternative<std::vector<ValueResolver>>(resolver));

    auto& resolverVec = std::get<std::vector<ValueResolver>>(resolver);
    EXPECT_EQ(resolverVec.size(), 3);
    EXPECT_EQ(std::get<std::string>(resolverVec[0]), "first");
    EXPECT_EQ(std::get<std::string>(resolverVec[1]), "second");
    EXPECT_EQ(std::get<std::string>(resolverVec[2]), "third");
}

TEST(Resolvers, SupportsCStringLiteral) {
    ValueResolver resolver("Hello");
    EXPECT_TRUE(std::holds_alternative<std::string>(resolver));
    EXPECT_EQ(std::get<std::string>(resolver), "Hello");

    const char* cstr = "World";
    ValueResolver resolver2(cstr);
    EXPECT_TRUE(std::holds_alternative<std::string>(resolver2));
    EXPECT_EQ(std::get<std::string>(resolver2), "World");
}
