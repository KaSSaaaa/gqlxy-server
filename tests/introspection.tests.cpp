#include <ariane/internal/introspection/introspection.h>
#include <ariane/internal/introspection/types/Document.h>
#include <ariane/schema.h>
#include <gtest/gtest.h>

using namespace std;
using namespace ariane::graphql;
using namespace ariane::graphql::internal;
using namespace ariane::graphql::internal;

TEST(Introspection, CreatesSchemaResolver) {
    Schema schema(SchemaOptions{.typeDefs = R"(
            type Query {
                hello: String
            }
        )",
                                .resolvers = {{"Query", Resolver{{"hello", "world"}}}}});

    auto schemaResolver = CreateSchemaResolver(schema.GetDocument());
    ASSERT_FALSE(schemaResolver.empty());
    ASSERT_TRUE(schemaResolver.contains("types"));
}

TEST(Introspection, SchemaHasTypes) {
    Schema schema(SchemaOptions{.typeDefs = R"(
            type Query {
                hello: String
            }
            type User {
                id: ID!
                name: String!
            }
        )",
                                .resolvers = {{"Query", Resolver{{"hello", "world"}}}}});

    auto schemaResolver = CreateSchemaResolver(schema.GetDocument());
    ASSERT_FALSE(schemaResolver.empty());
    ASSERT_TRUE(schemaResolver.contains("types"));
}

TEST(Introspection, CreatesTypeResolver) {
    TypeDefinition type;
    type.kind = TypeKind::OBJECT;
    type.name = "User";

    FieldDefinition field;
    field.name = "id";
    field.type = TypeRef::Named("ID");
    type.fields.push_back(field);

    auto typeResolver = CreateTypeResolver(type);
    ASSERT_FALSE(typeResolver.empty());
    ASSERT_TRUE(typeResolver.contains("name"));
}

TEST(Introspection, TypeResolverHasFields) {
    TypeDefinition type;
    type.kind = TypeKind::OBJECT;
    type.name = "User";

    FieldDefinition field1;
    field1.name = "id";
    field1.type = TypeRef::Named("ID");
    type.fields.push_back(field1);

    FieldDefinition field2;
    field2.name = "name";
    field2.type = TypeRef::Named("String");
    type.fields.push_back(field2);

    auto typeResolver = CreateTypeResolver(type);
    ASSERT_FALSE(typeResolver.empty());
    ASSERT_TRUE(typeResolver.contains("fields"));
}

TEST(Introspection, CreatesFieldResolver) {
    FieldDefinition field;
    field.name = "id";
    field.type = TypeRef::Named("ID");
    field.description = "User ID";

    auto fieldResolver = CreateFieldResolver(field);
    ASSERT_FALSE(fieldResolver.empty());
    ASSERT_TRUE(fieldResolver.contains("name"));
}

TEST(Introspection, FieldResolverHasTypeRef) {
    FieldDefinition field;
    field.name = "id";
    field.type = TypeRef::NonNull(TypeRef::Named("ID"));

    auto fieldResolver = CreateFieldResolver(field);
    ASSERT_FALSE(fieldResolver.empty());
    ASSERT_TRUE(fieldResolver.contains("type"));
}

TEST(Introspection, CreatesTypeRefResolver) {
    TypeRef typeRef = TypeRef::Named("String");
    auto typeRefResolver = CreateTypeRefResolver(typeRef);
    ASSERT_FALSE(typeRefResolver.empty());
    ASSERT_TRUE(typeRefResolver.contains("kind"));
}

TEST(Introspection, CreatesEnumValueResolver) {
    EnumValueDefinition enumValue;
    enumValue.name = "ADMIN";
    enumValue.description = "Administrator role";

    auto enumResolver = CreateEnumValueResolver(enumValue);
    ASSERT_FALSE(enumResolver.empty());
    ASSERT_TRUE(enumResolver.contains("name"));
}

TEST(Introspection, EnumTypeHasValues) {
    TypeDefinition type;
    type.kind = TypeKind::ENUM;
    type.name = "Role";

    EnumValueDefinition value1;
    value1.name = "ADMIN";
    type.enumValues.push_back(value1);

    EnumValueDefinition value2;
    value2.name = "USER";
    type.enumValues.push_back(value2);

    auto typeResolver = CreateTypeResolver(type);
    ASSERT_FALSE(typeResolver.empty());
    ASSERT_TRUE(typeResolver.contains("enumValues"));
}
