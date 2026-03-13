#include <ariane/ResolverArgs.h>
#include <ariane/resolvers.h>
#include <gtest/gtest.h>
#include <list>
#include <optional>

using namespace std;
using namespace ariane::graphql;

TEST(Resolvers, SupportsBasicTypes) {
    ValueResolver intResolver(42);
    EXPECT_TRUE(intResolver.Is<int>());
    EXPECT_EQ(intResolver.As<int>(), 42);

    ValueResolver uint64Resolver(uint64_t(42));
    EXPECT_TRUE(uint64Resolver.Is<uint64_t>());
    EXPECT_EQ(uint64Resolver.As<uint64_t>(), 42);

    ValueResolver stringResolver(string("Hello"));
    EXPECT_TRUE(stringResolver.Is<string>());
    EXPECT_EQ(stringResolver.As<string>(), "Hello");

    ValueResolver doubleResolver(3.14);
    EXPECT_TRUE(doubleResolver.Is<double>());
    EXPECT_DOUBLE_EQ(doubleResolver.As<double>(), 3.14);

    ValueResolver floatResolver(3.14f);
    EXPECT_TRUE(floatResolver.Is<float>());
    EXPECT_FLOAT_EQ(floatResolver.As<float>(), 3.14f);

    ValueResolver boolResolver(true);
    EXPECT_TRUE(boolResolver.Is<bool>());
    EXPECT_EQ(boolResolver.As<bool>(), true);
}

TEST(Resolvers, SupportsNullValues) {
    ValueResolver nullResolver(nullopt);
    EXPECT_TRUE(nullResolver.IsNull());

    ValueResolver monostateResolver(monostate{});
    EXPECT_TRUE(monostateResolver.IsNull());
}

TEST(Resolvers, SupportsOptionalTypes) {
    optional<int> presentInt = 42;
    ValueResolver resolver1(presentInt);
    EXPECT_TRUE(resolver1.Is<int>());
    EXPECT_EQ(resolver1.As<int>(), 42);

    optional<string> presentString = "Hello";
    ValueResolver resolver2(presentString);
    EXPECT_TRUE(resolver2.Is<string>());
    EXPECT_EQ(resolver2.As<string>(), "Hello");

    optional<int> absentInt = nullopt;
    ValueResolver resolver3(absentInt);
    EXPECT_TRUE(resolver3.IsNull());

    optional<string> absentString = nullopt;
    ValueResolver resolver4(absentString);
    EXPECT_TRUE(resolver4.IsNull());
}

TEST(Resolvers, SupportsFunctionResolvers) {
    auto funcResolver = [](const auto&) -> ValueResolver { return 42; };

    ValueResolver resolver(funcResolver);
    EXPECT_TRUE(resolver.Is<FunctionResolver>());
}

TEST(Resolvers, SupportsAsyncFunctionResolvers) {
    AsyncFunctionResolver asyncResolver = [](const ResolverArgs&) -> future<ValueResolver> {
        return async(launch::async, []() -> ValueResolver { return 42; });
    };

    ValueResolver resolver(asyncResolver);
    EXPECT_TRUE(resolver.Is<AsyncFunctionResolver>());
}

TEST(Resolvers, SupportsCoroutineResolvers) {
    CoroutineResolver coroutineResolver = [](const ResolverArgs&) -> Task<ValueResolver> { co_return 42; };

    ValueResolver resolver(coroutineResolver);
    EXPECT_TRUE(resolver.Is<CoroutineResolver>());

    auto& func = resolver.As<CoroutineResolver>();
    auto task = func(ResolverArgs{});
    ValueResolver result = task.get();
    EXPECT_TRUE(result.Is<int>());
    EXPECT_EQ(result.As<int>(), 42);
}

TEST(Resolvers, SupportsCallbackResolvers) {
    CallbackResolver callbackResolver = [](const ResolverArgs&, const function<void(const ValueResolver&)>& callback) {
        callback(42);
    };

    ValueResolver resolver(callbackResolver);
    EXPECT_TRUE(resolver.Is<CallbackResolver>());

    auto& func = resolver.As<CallbackResolver>();
    ValueResolver result;
    func(ResolverArgs{}, [&result](const ValueResolver& value) {
        result = value;
    });
    EXPECT_TRUE(result.Is<int>());
    EXPECT_EQ(result.As<int>(), 42);
}

TEST(Resolvers, SupportsOptionalFunctionForNullables) {
    auto optFunc = []() -> optional<ValueResolver> {
        return nullopt;
    };

    auto wrappedResolver = [optFunc](const ResolverArgs&) -> ValueResolver {
        auto result = optFunc();
        return result.value_or(monostate{});
    };

    ValueResolver resolver(wrappedResolver);
    EXPECT_TRUE(resolver.Is<FunctionResolver>());

    auto& func = resolver.As<FunctionResolver>();
    ValueResolver result = func(ResolverArgs{});
    EXPECT_TRUE(result.IsNull());
}

TEST(Resolvers, SupportsNestedResolvers) {
    Resolver nestedResolver = {
         {"id", ValueResolver(123)},
         {"name", ValueResolver(string("John"))},
         {"nullable", ValueResolver(nullopt)},
    };

    ValueResolver resolver(nestedResolver);
    EXPECT_TRUE(resolver.Is<Resolver>());

    auto& resolverList = resolver.As<Resolver>();
    unordered_map resolverMap(resolverList.begin(), resolverList.end());
    EXPECT_EQ(resolverMap.size(), 3);

    EXPECT_TRUE(resolverMap.at("nullable").IsNull());
}

TEST(Resolvers, SupportsVectorOfResolvers) {
    vector<ValueResolver> vec;
    vec.emplace_back(1);
    vec.emplace_back(2);
    vec.emplace_back(3);

    ValueResolver resolver(vec);
    EXPECT_TRUE(resolver.Is<vector<ValueResolver>>());

    auto& resolverVec = resolver.As<vector<ValueResolver>>();
    EXPECT_EQ(resolverVec.size(), 3);
    EXPECT_TRUE(resolverVec[0].Is<int>());
    EXPECT_EQ(resolverVec[0].As<int>(), 1);
    EXPECT_EQ(resolverVec[1].As<int>(), 2);
    EXPECT_EQ(resolverVec[2].As<int>(), 3);
}

TEST(Resolvers, SupportsInitializerListOfResolvers) {
    ValueResolver resolver({1, 2, 3});
    EXPECT_TRUE(resolver.Is<vector<ValueResolver>>());

    auto& resolverVec = resolver.As<vector<ValueResolver>>();
    EXPECT_EQ(resolverVec.size(), 3);
    EXPECT_EQ(resolverVec[0].As<int>(), 1);
    EXPECT_EQ(resolverVec[1].As<int>(), 2);
    EXPECT_EQ(resolverVec[2].As<int>(), 3);
}

TEST(Resolvers, SupportsListOfResolvers) {
    list<ValueResolver> list;
    list.emplace_back("first");
    list.emplace_back("second");
    list.emplace_back("third");

    ValueResolver resolver(list);
    EXPECT_TRUE(resolver.Is<vector<ValueResolver>>());

    auto& resolverVec = resolver.As<vector<ValueResolver>>();
    EXPECT_EQ(resolverVec.size(), 3);
    EXPECT_EQ(resolverVec[0].As<string>(), "first");
    EXPECT_EQ(resolverVec[1].As<string>(), "second");
    EXPECT_EQ(resolverVec[2].As<string>(), "third");
}

TEST(Resolvers, SupportsCStringLiteral) {
    ValueResolver resolver("Hello");
    EXPECT_TRUE(resolver.Is<string>());
    EXPECT_EQ(resolver.As<string>(), "Hello");

    const char* cstr = "World";
    ValueResolver resolver2(cstr);
    EXPECT_TRUE(resolver2.Is<string>());
    EXPECT_EQ(resolver2.As<string>(), "World");
}
