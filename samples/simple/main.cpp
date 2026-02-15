#include <iostream>
#include <map>
#include <string>
#include <functional>
#include <memory>
#include <ariane/graphql.h>

using namespace ariane::graphql;

int main()
{
    Resolver resolvers = {
        {"Query", {
            // {"hello", []() { return "Hello, world!"; }},
            {"user", Selector{
                {"id", 123},
                {"name", "John Doe"},
                {"email", "john@example.com"}
            }}
        }}
    };
    
    return 0;
}