#include <ariane/resolvers.h>
#include <ariane/schema.h>

#include <iostream>
#include <string>

using namespace std;
using namespace ariane::graphql;
using json = nlohmann::json;

template <std::ranges::input_range R>
auto to_string(R&& r) {
    return std::string(std::ranges::begin(r), std::ranges::end(r));
}

static void run(const string& label, Schema& schema, const string& query,
                const json& variables = json::object()) {
    cout << "--- " << label << " ---" << endl;
    auto result = schema.Resolve({.query = query, .variables = variables}).get();
    if (result.errors.has_value())
        for (const auto& e : result.errors.value())
            cerr << "Error: " << e.message << endl;
    else
        cout << json::parse(result.data.value()).dump(2) << endl;
    cout << endl;
}

int main() {
    // clang-format off
    Schema schema({
        .typeDefs = R"(
            type Query {
                name: String
                email: String
                role: String
                createdAt: String
            }
        )",
        .resolvers = {
            {"Query", Resolver{
                {"name",      "Alice"},
                {"email",     "alice@example.com"},
                {"role",      "admin"},
                {"createdAt", "2024-01-15T08:30:00Z"},
            }}
        },
        .directives = {
            {"redact", [](const ResolverArgs& args, const ValueResolver&) -> optional<ValueResolver> {
                return args.Args().value("if", false) ? nullopt : optional<ValueResolver>(monostate{});
            }},
            {"uppercase", [](const ResolverArgs&, const ValueResolver& v) -> optional<ValueResolver> {
                return to_string(get<string>(v) | views::transform(::toupper));
            }},
            {"prefix", [](const ResolverArgs& args, const ValueResolver& v) -> optional<ValueResolver> {
                return args.Args().value("with", string{}) + get<string>(v);
            }},
        }
    });
    // clang-format on

    run("@skip(if: true) omits field",
        schema, "{ name email @skip(if: true) role }");

    run("@include(if: false) omits field",
        schema, "{ name email @include(if: false) role }");

    run("@skip with variable",
        schema,
        "query($hideEmail: Boolean!) { name email @skip(if: $hideEmail) role }",
        {{"hideEmail", true}});

    run("custom @redact directive (skip-style)",
        schema, "{ name email @redact(if: true) role }");

    run("custom @uppercase transform",
        schema, "{ name @uppercase role }");

    run("custom @prefix transform with arg",
        schema, R"({ role @prefix(with: "role: ") })");

    return 0;
}
