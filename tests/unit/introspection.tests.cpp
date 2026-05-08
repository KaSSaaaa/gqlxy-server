#include <gqlxy/server/internal/introspection/introspection.h>
#include <gqlxy/server/internal/introspection/types/schema_definition.h>
#include <gqlxy/server/internal/peg/parser/schema_parser.h>
#include <gqlxy/server/schema.h>
#include <gtest/gtest.h>

using namespace std;
using namespace gqlxy;
using namespace gqlxy::internal;
using namespace gqlxy::parser;
using namespace gqlxy::utils;

class IntrospectionTests : public testing::Test {
protected:
    SchemaDefinition _emptySchema;
};

TEST_F(IntrospectionTests, CreatesSchemaResolver) {
    auto schemaDefinition = ParseSchemaDefinition(R"(
        type Query {
            hello: String
        }
    )");
    auto schemaResolver = CreateSchemaResolver(*schemaDefinition);
    ASSERT_FALSE(schemaResolver.empty());
    ASSERT_TRUE(schemaResolver.contains("types"));
}

TEST_F(IntrospectionTests, SchemaHasTypes) {
    auto schemaDefinition = ParseSchemaDefinition(R"(
        type Query {
            hello: String
        }
        type User {
            id: ID!
            name: String!
        }
    )");
    auto schemaResolver = CreateSchemaResolver(*schemaDefinition);
    ASSERT_FALSE(schemaResolver.empty());
    ASSERT_TRUE(schemaResolver.contains("types"));
}

TEST_F(IntrospectionTests, CreatesTypeResolver) {
    TypeDefinition type {
        .kind = TypeKind::OBJECT,
        .name = "User",
        .fields = {
            FieldDefinition {
                .name = "id",
                .type = TypeRef::Named("ID"),
            }
        }
    };

    auto typeResolver = CreateTypeResolver(type, _emptySchema);
    ASSERT_FALSE(typeResolver.empty());
    ASSERT_TRUE(typeResolver.contains("name"));
}

TEST_F(IntrospectionTests, TypeResolverHasFields) {
    TypeDefinition type {
        .kind = TypeKind::OBJECT,
        .name = "User",
        .fields = {
            FieldDefinition {
                .name = "id",
                .type = TypeRef::Named("ID"),
            },
            FieldDefinition {
                .name = "name",
                .type = TypeRef::Named("String"),
            }
        }
    };

    auto typeResolver = CreateTypeResolver(type, _emptySchema);
    ASSERT_FALSE(typeResolver.empty());
    ASSERT_TRUE(typeResolver.contains("fields"));
}

TEST_F(IntrospectionTests, CreatesFieldResolver) {
    FieldDefinition field {
        .name = "id",
        .description = "User ID",
        .type = TypeRef::Named("ID"),
    };

    auto fieldResolver = CreateFieldResolver(field, _emptySchema);
    ASSERT_FALSE(fieldResolver.empty());
    ASSERT_TRUE(fieldResolver.contains("name"));
}

TEST_F(IntrospectionTests, FieldResolverHasTypeRef) {
    FieldDefinition field {
        .name = "id",
        .type = TypeRef::NonNull(TypeRef::Named("ID"))
    };

    auto fieldResolver = CreateFieldResolver(field, _emptySchema);
    ASSERT_FALSE(fieldResolver.empty());
    ASSERT_TRUE(fieldResolver.contains("type"));
}

TEST_F(IntrospectionTests, CreatesTypeRefResolver) {
    TypeRef typeRef = TypeRef::Named("String");
    auto typeRefResolver = CreateTypeRefResolver(typeRef, _emptySchema);
    ASSERT_FALSE(typeRefResolver.empty());
    ASSERT_TRUE(typeRefResolver.contains("kind"));
}

TEST_F(IntrospectionTests, CreatesEnumValueResolver) {
    auto enumResolver = CreateEnumValueResolver(EnumValueDefinition {
        .name = "ADMIN",
        .description = "Administrator role"
    });

    ASSERT_FALSE(enumResolver.empty());
    ASSERT_TRUE(enumResolver.contains("name"));
}

TEST_F(IntrospectionTests, EnumTypeHasValues) {
    auto typeResolver = CreateTypeResolver(TypeDefinition {
        .name = "Role",
        .enumValues = {
            EnumValueDefinition {
                .name = "ADMIN",
            },
            EnumValueDefinition {
                .name = "USER",
            }
        }
    }, _emptySchema);
    ASSERT_FALSE(typeResolver.empty());
    ASSERT_TRUE(typeResolver.contains("enumValues"));
}
