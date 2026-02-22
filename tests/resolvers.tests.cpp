#include <ariane/resolvers.h>
#include <gtest/gtest.h>

#include <list>
#include <optional>

using namespace std;
using namespace ariane::graphql;

TEST(Resolvers, SupportsBasicTypes) {
    ValueResolver intResolver(42);
    EXPECT_TRUE(holds_alternative<int>(intResolver));
    EXPECT_EQ(get<int>(intResolver), 42);

    ValueResolver uint64Resolver(uint64_t(42));
    EXPECT_TRUE(holds_alternative<uint64_t>(uint64Resolver));
    EXPECT_EQ(get<uint64_t>(uint64Resolver), 42);

    ValueResolver stringResolver(string("Hello"));
    EXPECT_TRUE(holds_alternative<string>(stringResolver));
    EXPECT_EQ(get<string>(stringResolver), "Hello");

    ValueResolver doubleResolver(3.14);
    EXPECT_TRUE(holds_alternative<double>(doubleResolver));
    EXPECT_DOUBLE_EQ(get<double>(doubleResolver), 3.14);

    ValueResolver floatResolver(3.14f);
    EXPECT_TRUE(holds_alternative<float>(floatResolver));
    EXPECT_FLOAT_EQ(get<float>(floatResolver), 3.14f);

    ValueResolver boolResolver(true);
    EXPECT_TRUE(holds_alternative<bool>(boolResolver));
    EXPECT_EQ(get<bool>(boolResolver), true);
}

TEST(Resolvers, SupportsNullValues) {
    ValueResolver nullResolver(nullopt);
    EXPECT_TRUE(holds_alternative<monostate>(nullResolver));

    ValueResolver monostateResolver(monostate{});
    EXPECT_TRUE(holds_alternative<monostate>(monostateResolver));
}

TEST(Resolvers, SupportsOptionalTypes) {
    optional<int> presentInt = 42;
    ValueResolver resolver1(presentInt);
    EXPECT_TRUE(holds_alternative<int>(resolver1));
    EXPECT_EQ(get<int>(resolver1), 42);

    optional<string> presentString = "Hello";
    ValueResolver resolver2(presentString);
    EXPECT_TRUE(holds_alternative<string>(resolver2));
    EXPECT_EQ(get<string>(resolver2), "Hello");

    optional<int> absentInt = nullopt;
    ValueResolver resolver3(absentInt);
    EXPECT_TRUE(holds_alternative<monostate>(resolver3));

    optional<string> absentString = nullopt;
    ValueResolver resolver4(absentString);
    EXPECT_TRUE(holds_alternative<monostate>(resolver4));
}

TEST(Resolvers, SupportsFunctionResolvers) {
    auto funcResolver = []() -> ValueResolver { return 42; };

    ValueResolver resolver(funcResolver);
    EXPECT_TRUE(holds_alternative<FunctionResolver>(resolver));
}

TEST(Resolvers, SupportsAsyncFunctionResolvers) {
    auto asyncResolver = []() -> future<ValueResolver> {
        return async(launch::async, []() -> ValueResolver { return 42; });
    };

    ValueResolver resolver(asyncResolver);
    EXPECT_TRUE(holds_alternative<AsyncFunctionResolver>(resolver));
}

TEST(Resolvers, SupportsCoroutineResolvers) {
    auto coroutineResolver = []() -> Task<ValueResolver> { co_return 42; };

    ValueResolver resolver(coroutineResolver);
    EXPECT_TRUE(holds_alternative<CoroutineResolver>(resolver));

    auto& func = get<CoroutineResolver>(resolver);
    auto task = func();
    ValueResolver result = task.get();
    EXPECT_TRUE(holds_alternative<int>(result));
    EXPECT_EQ(get<int>(result), 42);
}

TEST(Resolvers, SupportsCallbackResolvers) {
    auto callbackResolver = [](const function<void(const ValueResolver&)>& callback) {
        callback(42);
    };

    ValueResolver resolver(callbackResolver);
    EXPECT_TRUE(holds_alternative<CallbackResolver>(resolver));

    auto& func = get<CallbackResolver>(resolver);
    ValueResolver result;
    func([&result](const ValueResolver& value) {
        result = value;
    });
    EXPECT_TRUE(holds_alternative<int>(result));
    EXPECT_EQ(get<int>(result), 42);
}

TEST(Resolvers, SupportsOptionalFunctionForNullables) {
    auto optFunc = []() -> optional<ValueResolver> {
        return nullopt;
    };

    auto wrappedResolver = [optFunc]() -> ValueResolver {
        auto result = optFunc();
        return result.value_or(monostate{});
    };

    ValueResolver resolver(wrappedResolver);
    EXPECT_TRUE(holds_alternative<FunctionResolver>(resolver));

    auto& func = get<FunctionResolver>(resolver);
    ValueResolver result = func();
    EXPECT_TRUE(holds_alternative<monostate>(result));
}

TEST(Resolvers, SupportsNestedResolvers) {
    Resolver nestedResolver = {
         {"id", ValueResolver(123)},
         {"name", ValueResolver(string("John"))},
         {"nullable", ValueResolver(nullopt)},
    };

    ValueResolver resolver(nestedResolver);
    EXPECT_TRUE(holds_alternative<Resolver>(resolver));

    auto& resolverList = get<Resolver>(resolver);
    unordered_map<string, ValueResolver> resolverMap(resolverList.begin(), resolverList.end());
    EXPECT_EQ(resolverMap.size(), 3);

    EXPECT_TRUE(holds_alternative<monostate>(resolverMap.at("nullable")));
}

TEST(Resolvers, SupportsVectorOfResolvers) {
    vector<ValueResolver> vec;
    vec.emplace_back(1);
    vec.emplace_back(2);
    vec.emplace_back(3);

    ValueResolver resolver(vec);
    EXPECT_TRUE(holds_alternative<vector<ValueResolver>>(resolver));

    auto& resolverVec = get<vector<ValueResolver>>(resolver);
    EXPECT_EQ(resolverVec.size(), 3);
    EXPECT_TRUE(holds_alternative<int>(resolverVec[0]));
    EXPECT_EQ(get<int>(resolverVec[0]), 1);
    EXPECT_EQ(get<int>(resolverVec[1]), 2);
    EXPECT_EQ(get<int>(resolverVec[2]), 3);
}

TEST(Resolvers, SupportsInitializerListOfResolvers) {
    ValueResolver resolver({1, 2, 3});
    EXPECT_TRUE(holds_alternative<vector<ValueResolver>>(resolver));

    auto& resolverVec = get<vector<ValueResolver>>(resolver);
    EXPECT_EQ(resolverVec.size(), 3);
    EXPECT_EQ(get<int>(resolverVec[0]), 1);
    EXPECT_EQ(get<int>(resolverVec[1]), 2);
    EXPECT_EQ(get<int>(resolverVec[2]), 3);
}

TEST(Resolvers, SupportsListOfResolvers) {
    list<ValueResolver> list;
    list.emplace_back("first");
    list.emplace_back("second");
    list.emplace_back("third");

    ValueResolver resolver(list);
    EXPECT_TRUE(holds_alternative<vector<ValueResolver>>(resolver));

    auto& resolverVec = get<vector<ValueResolver>>(resolver);
    EXPECT_EQ(resolverVec.size(), 3);
    EXPECT_EQ(get<string>(resolverVec[0]), "first");
    EXPECT_EQ(get<string>(resolverVec[1]), "second");
    EXPECT_EQ(get<string>(resolverVec[2]), "third");
}

TEST(Resolvers, SupportsCStringLiteral) {
    ValueResolver resolver("Hello");
    EXPECT_TRUE(holds_alternative<string>(resolver));
    EXPECT_EQ(get<string>(resolver), "Hello");

    const char* cstr = "World";
    ValueResolver resolver2(cstr);
    EXPECT_TRUE(holds_alternative<string>(resolver2));
    EXPECT_EQ(get<string>(resolver2), "World");
}
