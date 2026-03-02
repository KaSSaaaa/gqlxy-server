#include <ariane/internal/ast/OperationDefinition.h>
#include <ariane/internal/peg/parser/query/ParseOperations.h>
#include <gtest/gtest.h>

using namespace ariane::graphql::internal;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static const Field& asField(const Selection& s) {
    return std::get<Field>(s);
}

static const FragmentSpread& asFragmentSpread(const Selection& s) {
    return std::get<FragmentSpread>(s);
}

static const InlineFragment& asInlineFragment(const Selection& s) {
    return std::get<InlineFragment>(s);
}

// ---------------------------------------------------------------------------
// ParseOperations — operation type and name
// ---------------------------------------------------------------------------

TEST(QueryParser, ParsesShorthandQuery) {
    auto ops = ParseOperations("{ hero }");
    ASSERT_EQ(ops.size(), 1);
    EXPECT_EQ(ops[0].type._value, OperationType::QUERY);
    EXPECT_FALSE(ops[0].name.has_value());
}

TEST(QueryParser, ParsesNamedQuery) {
    auto ops = ParseOperations("query GetHero { hero }");
    ASSERT_EQ(ops.size(), 1);
    EXPECT_EQ(ops[0].type._value, OperationType::QUERY);
    ASSERT_TRUE(ops[0].name.has_value());
    EXPECT_EQ(*ops[0].name, "GetHero");
}

TEST(QueryParser, ParsesMutation) {
    auto ops = ParseOperations("mutation CreateUser { createUser }");
    ASSERT_EQ(ops.size(), 1);
    EXPECT_EQ(ops[0].type._value, OperationType::MUTATION);
    EXPECT_EQ(*ops[0].name, "CreateUser");
}

TEST(QueryParser, ParsesSubscription) {
    auto ops = ParseOperations("subscription OnMessage { messageAdded }");
    ASSERT_EQ(ops.size(), 1);
    EXPECT_EQ(ops[0].type._value, OperationType::SUBSCRIPTION);
    EXPECT_EQ(*ops[0].name, "OnMessage");
}

TEST(QueryParser, ReturnsEmptyOnInvalidQuery) {
    auto ops = ParseOperations("not valid graphql @@@@");
    EXPECT_EQ(ops.size(), 0);
}

// ---------------------------------------------------------------------------
// ParseOperations — selection sets and fields
// ---------------------------------------------------------------------------

TEST(QueryParser, ParsesSingleField) {
    auto ops = ParseOperations("{ hero }");
    ASSERT_EQ(ops[0].selectionSet.selections.size(), 1);
    EXPECT_EQ(asField(ops[0].selectionSet.selections[0]).name, "hero");
}

TEST(QueryParser, ParsesMultipleFields) {
    auto ops = ParseOperations("{ hero villain sidekick }");
    const auto& selections = ops[0].selectionSet.selections;
    ASSERT_EQ(selections.size(), 3);
    EXPECT_EQ(asField(selections[0]).name, "hero");
    EXPECT_EQ(asField(selections[1]).name, "villain");
    EXPECT_EQ(asField(selections[2]).name, "sidekick");
}

TEST(QueryParser, ParsesFieldAlias) {
    auto ops = ParseOperations("{ myHero: hero }");
    const auto& field = asField(ops[0].selectionSet.selections[0]);
    ASSERT_TRUE(field.alias.has_value());
    EXPECT_EQ(*field.alias, "myHero");
    EXPECT_EQ(field.name, "hero");
}

TEST(QueryParser, FieldWithNoAliasHasEmptyOptional) {
    auto ops = ParseOperations("{ hero }");
    const auto& field = asField(ops[0].selectionSet.selections[0]);
    EXPECT_FALSE(field.alias.has_value());
}

TEST(QueryParser, ParsesNestedSelectionSet) {
    auto ops = ParseOperations("{ hero { id name } }");
    const auto& hero = asField(ops[0].selectionSet.selections[0]);
    ASSERT_TRUE(hero.selectionSet != nullptr);
    ASSERT_EQ(hero.selectionSet->selections.size(), 2);
    EXPECT_EQ(asField(hero.selectionSet->selections[0]).name, "id");
    EXPECT_EQ(asField(hero.selectionSet->selections[1]).name, "name");
}

TEST(QueryParser, FieldWithNoSubselectionHasNullSelectionSet) {
    auto ops = ParseOperations("{ hero }");
    const auto& field = asField(ops[0].selectionSet.selections[0]);
    EXPECT_EQ(field.selectionSet, nullptr);
}

// ---------------------------------------------------------------------------
// ParseOperations — arguments
// ---------------------------------------------------------------------------

TEST(QueryParser, ParsesStringArgument) {
    auto ops = ParseOperations(R"({ hero(id: "42") })");
    const auto& field = asField(ops[0].selectionSet.selections[0]);
    ASSERT_EQ(field.arguments.size(), 1);
    EXPECT_EQ(field.arguments[0].name, "id");
    EXPECT_FALSE(field.arguments[0].value.empty());
}

TEST(QueryParser, ParsesIntArgument) {
    auto ops = ParseOperations("{ hero(id: 42) }");
    const auto& field = asField(ops[0].selectionSet.selections[0]);
    ASSERT_EQ(field.arguments.size(), 1);
    EXPECT_EQ(field.arguments[0].name, "id");
    EXPECT_EQ(field.arguments[0].value, "42");
}

TEST(QueryParser, ParsesBoolArgument) {
    auto ops = ParseOperations("{ hero(active: true) }");
    const auto& field = asField(ops[0].selectionSet.selections[0]);
    ASSERT_EQ(field.arguments.size(), 1);
    EXPECT_EQ(field.arguments[0].name, "active");
    EXPECT_EQ(field.arguments[0].value, "true");
}

TEST(QueryParser, ParsesEnumArgument) {
    auto ops = ParseOperations("{ hero(episode: JEDI) }");
    const auto& field = asField(ops[0].selectionSet.selections[0]);
    ASSERT_EQ(field.arguments.size(), 1);
    EXPECT_EQ(field.arguments[0].name, "episode");
    EXPECT_EQ(field.arguments[0].value, "JEDI");
}

TEST(QueryParser, ParsesVariableArgument) {
    auto ops = ParseOperations("query($id: ID!) { hero(id: $id) }");
    const auto& field = asField(ops[0].selectionSet.selections[0]);
    ASSERT_EQ(field.arguments.size(), 1);
    EXPECT_EQ(field.arguments[0].name, "id");
    EXPECT_EQ(field.arguments[0].value, "$id");
}

TEST(QueryParser, ParsesMultipleArguments) {
    auto ops = ParseOperations("{ search(query: \"Luke\", limit: 10) }");
    const auto& field = asField(ops[0].selectionSet.selections[0]);
    ASSERT_EQ(field.arguments.size(), 2);
    EXPECT_EQ(field.arguments[0].name, "query");
    EXPECT_EQ(field.arguments[1].name, "limit");
}

// ---------------------------------------------------------------------------
// ParseOperations — variable definitions
// ---------------------------------------------------------------------------

TEST(QueryParser, ParsesSingleVariableDefinition) {
    auto ops = ParseOperations("query($id: ID!) { hero }");
    ASSERT_EQ(ops[0].variableDefinitions.size(), 1);
    EXPECT_EQ(ops[0].variableDefinitions[0].name, "id");
}

TEST(QueryParser, ParsesMultipleVariableDefinitions) {
    auto ops = ParseOperations("query($id: ID! $limit: Int) { hero }");
    ASSERT_EQ(ops[0].variableDefinitions.size(), 2);
    EXPECT_EQ(ops[0].variableDefinitions[0].name, "id");
    EXPECT_EQ(ops[0].variableDefinitions[1].name, "limit");
}

TEST(QueryParser, ParsesVariableTypeNamed) {
    auto ops = ParseOperations("query($id: ID) { hero }");
    ASSERT_EQ(ops[0].variableDefinitions.size(), 1);
    EXPECT_EQ(ops[0].variableDefinitions[0].type.typeName(), "ID");
}

TEST(QueryParser, ParsesVariableTypeNonNull) {
    auto ops = ParseOperations("query($id: ID!) { hero }");
    const auto& varDef = ops[0].variableDefinitions[0];
    EXPECT_EQ(varDef.type.kind._value, TypeRefKind::NON_NULL);
    EXPECT_EQ(varDef.type.typeName(), "ID");
}

TEST(QueryParser, ParsesVariableWithDefaultValue) {
    auto ops = ParseOperations("query($limit: Int = 10) { hero }");
    ASSERT_EQ(ops[0].variableDefinitions.size(), 1);
    ASSERT_TRUE(ops[0].variableDefinitions[0].defaultValue.has_value());
    EXPECT_EQ(*ops[0].variableDefinitions[0].defaultValue, "10");
}

TEST(QueryParser, ParsesVariableWithNoDefaultValue) {
    auto ops = ParseOperations("query($id: ID!) { hero }");
    EXPECT_FALSE(ops[0].variableDefinitions[0].defaultValue.has_value());
}

// ---------------------------------------------------------------------------
// ParseOperations — fragment spreads
// ---------------------------------------------------------------------------

TEST(QueryParser, ParsesFragmentSpread) {
    auto ops = ParseOperations("{ hero { ...HeroFields } }");
    const auto& hero = asField(ops[0].selectionSet.selections[0]);
    ASSERT_EQ(hero.selectionSet->selections.size(), 1);
    const auto& spread = asFragmentSpread(hero.selectionSet->selections[0]);
    EXPECT_EQ(spread.name, "HeroFields");
}

// ---------------------------------------------------------------------------
// ParseOperations — inline fragments
// ---------------------------------------------------------------------------

TEST(QueryParser, ParsesInlineFragmentWithTypeCondition) {
    auto ops = ParseOperations("{ hero { ... on Droid { primaryFunction } } }");
    const auto& hero = asField(ops[0].selectionSet.selections[0]);
    ASSERT_EQ(hero.selectionSet->selections.size(), 1);
    const auto& inlineFrag = asInlineFragment(hero.selectionSet->selections[0]);
    ASSERT_TRUE(inlineFrag.typeCondition.has_value());
    EXPECT_EQ(*inlineFrag.typeCondition, "Droid");
    ASSERT_TRUE(inlineFrag.selectionSet != nullptr);
    EXPECT_EQ(asField(inlineFrag.selectionSet->selections[0]).name, "primaryFunction");
}

TEST(QueryParser, ParsesInlineFragmentWithoutTypeCondition) {
    auto ops = ParseOperations("{ hero { ... { id } } }");
    const auto& hero = asField(ops[0].selectionSet.selections[0]);
    ASSERT_EQ(hero.selectionSet->selections.size(), 1);
    const auto& inlineFrag = asInlineFragment(hero.selectionSet->selections[0]);
    EXPECT_FALSE(inlineFrag.typeCondition.has_value());
}

// ---------------------------------------------------------------------------
// ParseOperations — directives
// ---------------------------------------------------------------------------

TEST(QueryParser, ParsesFieldDirectiveWithBoolArg) {
    auto ops = ParseOperations("query($skip: Boolean!) { hero @skip(if: $skip) }");
    const auto& field = asField(ops[0].selectionSet.selections[0]);
    ASSERT_EQ(field.directives.size(), 1);
    EXPECT_EQ(field.directives[0].name, "skip");
    ASSERT_EQ(field.directives[0].args.size(), 1);
    EXPECT_EQ(field.directives[0].args[0].name, "if");
    EXPECT_EQ(field.directives[0].args[0].value, "$skip");
}

TEST(QueryParser, ParsesFieldDirectiveWithLiteralBoolArg) {
    auto ops = ParseOperations("{ hero @include(if: true) }");
    const auto& field = asField(ops[0].selectionSet.selections[0]);
    ASSERT_EQ(field.directives.size(), 1);
    EXPECT_EQ(field.directives[0].name, "include");
    EXPECT_EQ(field.directives[0].args[0].value, "true");
}

TEST(QueryParser, FieldWithNoDirectivesHasEmptyVector) {
    auto ops = ParseOperations("{ hero }");
    const auto& field = asField(ops[0].selectionSet.selections[0]);
    EXPECT_TRUE(field.directives.empty());
}

TEST(QueryParser, ParsesMultipleDirectivesOnField) {
    auto ops = ParseOperations("{ hero @skip(if: true) @include(if: false) }");
    const auto& field = asField(ops[0].selectionSet.selections[0]);
    ASSERT_EQ(field.directives.size(), 2);
    EXPECT_EQ(field.directives[0].name, "skip");
    EXPECT_EQ(field.directives[1].name, "include");
}

TEST(QueryParser, ParsesDirectiveOnFragmentSpread) {
    auto ops = ParseOperations("{ hero { ...HeroFields @skip(if: true) } }");
    const auto& hero = asField(ops[0].selectionSet.selections[0]);
    const auto& spread = asFragmentSpread(hero.selectionSet->selections[0]);
    ASSERT_EQ(spread.directives.size(), 1);
    EXPECT_EQ(spread.directives[0].name, "skip");
}

TEST(QueryParser, ParsesDirectiveOnInlineFragment) {
    auto ops = ParseOperations("{ hero { ... on Droid @skip(if: true) { id } } }");
    const auto& hero = asField(ops[0].selectionSet.selections[0]);
    const auto& frag = asInlineFragment(hero.selectionSet->selections[0]);
    ASSERT_EQ(frag.directives.size(), 1);
    EXPECT_EQ(frag.directives[0].name, "skip");
}
