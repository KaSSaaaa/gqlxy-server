#include <gqlxy/server/internal/introspection/types/schema_definition.h>
#include <gqlxy/server/internal/peg/parser/schema_parser.h>
#include <gqlxy/server/schema.h>
#include <gtest/gtest.h>

using namespace std;
using namespace gqlxy;
using namespace gqlxy::internal;
using namespace gqlxy::parser;
using namespace gqlxy::utils;

TEST(SchemaParser, ParsesObjectTypes) {
    auto schemaDefinition = ParseSchemaDefinition(R"(
        type Query {
            hello: String
        }
        type User {
            id: ID!
            name: String!
            email: String
        }
    )");

    ASSERT_EQ(schemaDefinition->types.size(), 2);
    ASSERT_TRUE(schemaDefinition->types.contains("User"));

    const auto& userType = schemaDefinition->types.at("User");
    EXPECT_EQ(userType.name, "User");
    EXPECT_EQ(userType.fields.size(), 3);
}

TEST(SchemaParser, ParsesFieldTypes) {
    const auto& schemaDefinition = ParseSchemaDefinition(R"(
        type Query {
            hello: String
        }
        type User {
            id: ID!
            email: String
            tags: [String]
        }
    )");
    const auto& userType = schemaDefinition->types.at("User");

    auto idField = ranges::find_if(userType.fields, [](const FieldDefinition& f) { return f.name == "id"; });
    ASSERT_NE(idField, userType.fields.end());
    EXPECT_EQ(idField->type.kind._value, TypeRefKind::NON_NULL);
    EXPECT_EQ(idField->type.ofType->kind._value, TypeRefKind::NamedType);
    EXPECT_EQ(idField->type.ofType->name, "ID");

    auto emailField = ranges::find_if(userType.fields, [](const FieldDefinition& f) { return f.name == "email"; });
    ASSERT_NE(emailField, userType.fields.end());
    EXPECT_EQ(emailField->type.kind._value, TypeRefKind::NamedType);
    EXPECT_EQ(emailField->type.name, "String");

    auto tagsField = ranges::find_if(userType.fields, [](const FieldDefinition& f) { return f.name == "tags"; });
    ASSERT_NE(tagsField, userType.fields.end());
    EXPECT_EQ(tagsField->type.kind._value, TypeRefKind::LIST);
}

TEST(SchemaParser, ParsesScalarTypes) {
    auto schemaDefinition = ParseSchemaDefinition(R"(
        type Query {
            hello: String
        }
        scalar DateTime
    )");
    ASSERT_TRUE(schemaDefinition->types.contains("DateTime"));

    const auto& dateTimeType = schemaDefinition->types.at("DateTime");
    EXPECT_EQ(dateTimeType.kind._value, TypeKind::SCALAR);
    EXPECT_EQ(dateTimeType.name, "DateTime");
}

TEST(SchemaParser, ParsesEnumTypes) {
    auto schemaDefinition = ParseSchemaDefinition(R"(
        type Query {
            hello: String
        }
        enum Role {
            ADMIN
            USER
            GUEST
        }
    )");
    ASSERT_TRUE(schemaDefinition->types.contains("Role"));

    const auto& roleType = schemaDefinition->types.at("Role");
    EXPECT_EQ(roleType.kind._value, TypeKind::ENUM);
    EXPECT_EQ(roleType.name, "Role");
    EXPECT_EQ(roleType.enumValues.size(), 3);
    EXPECT_EQ(roleType.enumValues[0].name, "ADMIN");
    EXPECT_EQ(roleType.enumValues[1].name, "USER");
    EXPECT_EQ(roleType.enumValues[2].name, "GUEST");
}

TEST(SchemaParser, ParsesInterfaceTypes) {
    auto schemaDefinition = ParseSchemaDefinition(R"(
        type Query {
            hello: String
        }
        interface Node {
            id: ID!
        }
    )");
    ASSERT_TRUE(schemaDefinition->types.contains("Node"));

    const auto& nodeType = schemaDefinition->types.at("Node");
    EXPECT_EQ(nodeType.kind._value, TypeKind::INTERFACE);
    EXPECT_EQ(nodeType.name, "Node");
    EXPECT_EQ(nodeType.fields.size(), 1);
}

TEST(SchemaParser, ParsesUnionTypes) {
    auto schemaDefinition = ParseSchemaDefinition(R"(
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
    )");
    ASSERT_TRUE(schemaDefinition->types.contains("Pet"));

    const auto& petType = schemaDefinition->types.at("Pet");
    EXPECT_EQ(petType.kind._value, TypeKind::UNION);
    EXPECT_EQ(petType.name, "Pet");
    EXPECT_EQ(petType.unionTypes.size(), 2);
}

TEST(SchemaParser, ExtendsObjectTypeWithFields) {
    auto schemaDefinition = ParseSchemaDefinition(R"(
        type User {
            id: ID!
            name: String
        }
        extend type User {
            email: String
            age: Int
        }
    )");
    ASSERT_TRUE(schemaDefinition->types.contains("User"));

    const auto& user = schemaDefinition->types.at("User");
    EXPECT_EQ(user.fields.size(), 4);
    EXPECT_EQ(user.fields[2].name, "email");
    EXPECT_EQ(user.fields[3].name, "age");
}

TEST(SchemaParser, ExtendsObjectTypeWithInterfaces) {
    auto schemaDefinition = ParseSchemaDefinition(R"(
        interface Node {
            id: ID!
        }
        type User {
            id: ID!
            name: String
        }
        extend type User implements Node {
            email: String
        }
    )");
    ASSERT_TRUE(schemaDefinition->types.contains("User"));

    const auto& user = schemaDefinition->types.at("User");
    ASSERT_EQ(user.interfaces.size(), 1);
    EXPECT_EQ(user.interfaces[0], "Node");
    EXPECT_EQ(user.fields.size(), 3);

    const auto& node = schemaDefinition->types.at("Node");
    ASSERT_EQ(node.possibleTypes.size(), 1);
    EXPECT_EQ(node.possibleTypes[0], "User");
}

TEST(SchemaParser, ExtendsInterfaceWithFields) {
    auto schemaDefinition = ParseSchemaDefinition(R"(
        interface Node {
            id: ID!
        }
        extend interface Node {
            createdAt: String
        }
    )");
    ASSERT_TRUE(schemaDefinition->types.contains("Node"));

    const auto& node = schemaDefinition->types.at("Node");
    EXPECT_EQ(node.fields.size(), 2);
    EXPECT_EQ(node.fields[1].name, "createdAt");
}

TEST(SchemaParser, ExtendsUnionType) {
    auto schemaDefinition = ParseSchemaDefinition(R"(
        type Dog { name: String }
        type Cat { name: String }
        type Fish { name: String }
        union Pet = Dog | Cat
        extend union Pet = Fish
    )");
    ASSERT_TRUE(schemaDefinition->types.contains("Pet"));

    const auto& pet = schemaDefinition->types.at("Pet");
    EXPECT_EQ(pet.unionTypes.size(), 3);
    EXPECT_EQ(pet.unionTypes[2], "Fish");
}

TEST(SchemaParser, ExtendsEnumType) {
    auto schemaDefinition = ParseSchemaDefinition(R"(
        enum Status { ACTIVE INACTIVE }
        extend enum Status { PENDING }
    )");
    ASSERT_TRUE(schemaDefinition->types.contains("Status"));

    const auto& status = schemaDefinition->types.at("Status");
    EXPECT_EQ(status.enumValues.size(), 3);
    EXPECT_EQ(status.enumValues[2].name, "PENDING");
}

TEST(SchemaParser, ExtendsInputType) {
    auto schemaDefinition = ParseSchemaDefinition(R"(
        input CreateUser { name: String }
        extend input CreateUser { email: String }
    )");
    ASSERT_TRUE(schemaDefinition->types.contains("CreateUser"));

    const auto& createUser = schemaDefinition->types.at("CreateUser");
    EXPECT_EQ(createUser.inputFields.size(), 2);
    EXPECT_EQ(createUser.inputFields[1].name, "email");
}

TEST(SchemaParser, ExtendUnknownTypeIsIgnored) {
    auto schemaDefinition = ParseSchemaDefinition(R"(
        type User { id: ID! }
        extend type Ghost { name: String }
    )");
    EXPECT_FALSE(schemaDefinition->types.contains("Ghost"));
    EXPECT_EQ(schemaDefinition->types.at("User").fields.size(), 1);
}
