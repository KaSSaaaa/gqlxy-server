# ariane-graphql-server
An unopinionated C++ GraphQL Server

## What is Ariane ?
When I thought about Ariane, it was made through frustration at first. I wanted to make a GraphQL server in C++ (because I'm stuck with it within legacy code).

The options I had were quickly limited : cppgraphqlgen and libgraphqlparser. The first option is complex and didn't fit my needs. The second, well, is just a parser.

I wanted something way simpler, kind of like what Apollo does for servers in the JS ecosystem. I wanted to be able to do that :

```cpp
Resolvers resolvers = {
    {"Query", {
        {"hello", []() { return "Hello, world!"; }},
        {"user", {
            {"id", 123},
            {"name", "John Doe"},
            {"email", "john@example.com"}
        }}
    }}
};
```

So, like a normal C++ dev, I wrote it myself.

Ariane is a C++, minimalist, unopinionated GraphQL server that lets you build the GraphQL server you want, however you want. Without forcing you into modern paradigms like future, coroutines, ...<br/>
With Ariane, you build it however you want it. Want callbacks ? You can do it. Futures ? Same. Coroutines ? Same. Use Boost/Qt signals under the hood ? Feel free.

Ariane doesn't care how your server works under the hood. It helps you leverage its potential with simplicity.