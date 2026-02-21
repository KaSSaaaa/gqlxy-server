#include <ariane/schema.h>
#include <gtest/gtest.h>

using namespace ariane::graphql;

TEST(SchemaParser, ParsesObjectTypes) {
    Schema schema(SchemaOptions{.typeDefs = R"(
            type Query {
                hello: String
            }
            type User {
                id: ID!
                name: String!
                email: String
            }
        )",
                                .resolvers = {{"Query", Resolver{{"hello", "world"}}}}});

    const auto& doc = schema.GetDocument();
    ASSERT_EQ(doc.types.size(), 2);
    ASSERT_TRUE(doc.types.find("User") != doc.types.end());

    const auto& userType = doc.types.at("User");
    EXPECT_EQ(userType.name, "User");
    EXPECT_EQ(userType.fields.size(), 3);
}

TEST(SchemaParser, ParsesFieldTypes) {
    Schema schema(SchemaOptions{.typeDefs = R"(
            type Query {
                hello: String
            }
            type User {
                id: ID!
                email: String
                tags: [String]
            }
        )",
                                .resolvers = {{"Query", Resolver{{"hello", "world"}}}}});

    const auto& doc = schema.GetDocument();
    const auto& userType = doc.types.at("User");

    auto idField = std::find_if(userType.fields.begin(), userType.fields.end(),
                                [](const FieldDefinition& f) { return f.name == "id"; });
    ASSERT_NE(idField, userType.fields.end());
    EXPECT_EQ(idField->type.kind._value, TypeRefKind::NonNull);
    EXPECT_EQ(idField->type.ofType->kind._value, TypeRefKind::NamedType);
    EXPECT_EQ(idField->type.ofType->name, "ID");

    auto emailField = std::find_if(userType.fields.begin(), userType.fields.end(),
                                   [](const FieldDefinition& f) { return f.name == "email"; });
    ASSERT_NE(emailField, userType.fields.end());
    EXPECT_EQ(emailField->type.kind._value, TypeRefKind::NamedType);
    EXPECT_EQ(emailField->type.name, "String");

    auto tagsField = std::find_if(userType.fields.begin(), userType.fields.end(),
                                  [](const FieldDefinition& f) { return f.name == "tags"; });
    ASSERT_NE(tagsField, userType.fields.end());
    EXPECT_EQ(tagsField->type.kind._value, TypeRefKind::List);
}

TEST(SchemaParser, ParsesScalarTypes) {
    Schema schema(SchemaOptions{.typeDefs = R"(
            type Query {
                hello: String
            }
            scalar DateTime
        )",
                                .resolvers = {{"Query", Resolver{{"hello", "world"}}}}});

    const auto& doc = schema.GetDocument();
    ASSERT_TRUE(doc.types.find("DateTime") != doc.types.end());

    const auto& dateTimeType = doc.types.at("DateTime");
    EXPECT_EQ(dateTimeType.kind._value, TypeKind::SCALAR);
    EXPECT_EQ(dateTimeType.name, "DateTime");
}

TEST(SchemaParser, ParsesEnumTypes) {
    Schema schema(SchemaOptions{.typeDefs = R"(
            type Query {
                hello: String
            }
            enum Role {
                ADMIN
                USER
                GUEST
            }
        )",
                                .resolvers = {{"Query", Resolver{{"hello", "world"}}}}});

    const auto& doc = schema.GetDocument();
    ASSERT_TRUE(doc.types.find("Role") != doc.types.end());

    const auto& roleType = doc.types.at("Role");
    EXPECT_EQ(roleType.kind._value, TypeKind::ENUM);
    EXPECT_EQ(roleType.name, "Role");
    EXPECT_EQ(roleType.enumValues.size(), 3);
    EXPECT_EQ(roleType.enumValues[0].name, "ADMIN");
    EXPECT_EQ(roleType.enumValues[1].name, "USER");
    EXPECT_EQ(roleType.enumValues[2].name, "GUEST");
}

TEST(SchemaParser, ParsesInterfaceTypes) {
    Schema schema(SchemaOptions{.typeDefs = R"(
            type Query {
                hello: String
            }
            interface Node {
                id: ID!
            }
        )",
                                .resolvers = {{"Query", Resolver{{"hello", "world"}}}}});

    const auto& doc = schema.GetDocument();
    ASSERT_TRUE(doc.types.find("Node") != doc.types.end());

    const auto& nodeType = doc.types.at("Node");
    EXPECT_EQ(nodeType.kind._value, TypeKind::INTERFACE);
    EXPECT_EQ(nodeType.name, "Node");
    EXPECT_EQ(nodeType.fields.size(), 1);
}

TEST(SchemaParser, ParsesUnionTypes) {
    Schema schema(SchemaOptions{.typeDefs = R"(
            type Query {
                hello: String
            }
            type Dog {
                name: String
            }
            type Cat {
                name: String
            }
            union Pet = Dog | Cat
        )",
                                .resolvers = {{"Query", Resolver{{"hello", "world"}}}}});

    const auto& doc = schema.GetDocument();
    ASSERT_TRUE(doc.types.find("Pet") != doc.types.end());

    const auto& petType = doc.types.at("Pet");
    EXPECT_EQ(petType.kind._value, TypeKind::UNION);
    EXPECT_EQ(petType.name, "Pet");
    EXPECT_EQ(petType.unionTypes.size(), 2);
}
