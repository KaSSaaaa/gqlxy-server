#include <ariane/introspection.h>
#include <ariane/schema.h>
#include <iostream>
#include <nlohmann/json.hpp>

using namespace ariane::graphql;
using json = nlohmann::json;

int main() {
    std::cout << "=== GraphQL Introspection Query Example ===" << std::endl << std::endl;

    Schema schema(SchemaOptions{.typeDefs = R"(
            type Query {
                hello: String
                user(id: ID!): User
            }

            type User {
                id: ID!
                name: String!
                email: String
                role: Role!
            }

            enum Role {
                ADMIN
                USER
                GUEST
            }
        )",
                                .resolvers = {{"Query", Resolver{{"hello", "world"},
                                                                 {"user", Resolver{{"id", "123"},
                                                                                   {"name", "John Doe"},
                                                                                   {"email", "john@example.com"},
                                                                                   {"role", "USER"}}}}}}});

    std::string introspectionQuery = R"(
        query {
            __schema {
                queryType { name }
                mutationType { name }
                subscriptionType { name }
                types {
                    kind
                    name
                    description
                    fields {
                        name
                        description
                        args {
                            name
                            type {
                                kind
                                name
                                ofType {
                                    kind
                                    name
                                }
                            }
                        }
                        type {
                            kind
                            name
                            ofType {
                                kind
                                name
                                ofType {
                                    kind
                                    name
                                }
                            }
                        }
                        isDeprecated
                        deprecationReason
                    }
                    enumValues {
                        name
                        description
                        isDeprecated
                        deprecationReason
                    }
                    interfaces {
                        kind
                        name
                    }
                    possibleTypes {
                        kind
                        name
                    }
                }
                directives {
                    name
                    description
                    locations
                    args {
                        name
                    }
                }
            }
        }
    )";

    std::cout << "Executing introspection query..." << std::endl << std::endl;
    auto result = schema.Resolve(introspectionQuery);

    try {
        auto j = json::parse(result.data);
        std::cout << j.dump(2) << std::endl;
    } catch (const json::parse_error& e) {
        std::cout << "JSON parse error: " << e.what() << std::endl;
        std::cout << "Raw result:" << std::endl;
        std::cout << result.data << std::endl;
    }

    std::cout << std::endl << "=== Regular Query with __typename ===" << std::endl << std::endl;

    std::string regularQuery = R"(
        query {
            __typename
            hello
            user(id: "123") {
                id
                name
                email
            }
        }
    )";

    auto regularResult = schema.Resolve(regularQuery);
    try {
        auto j = json::parse(regularResult.data);
        std::cout << j.dump(2) << std::endl;
    } catch (const json::parse_error& e) {
        std::cout << "JSON parse error: " << e.what() << std::endl;
        std::cout << "Raw result:" << std::endl;
        std::cout << regularResult.data << std::endl;
    }

    return 0;
}
