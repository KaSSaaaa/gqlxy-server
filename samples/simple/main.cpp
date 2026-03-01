#include <ariane/resolvers.h>
#include <ariane/schema.h>
#include <ariane/task.h>
#include <nlohmann/json.hpp>

#include <fstream>
#include <future>
#include <iostream>
#include <sstream>
#include <string>

using namespace std;
using namespace ariane::graphql;
using json = nlohmann::json;

static string readFile(const char* path) {
    ifstream f(path);
    if (!f) {
        throw runtime_error(string("Cannot open schema file: ") + path);
    }
    ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

int main() {
    cout << "=== Ariane — schema.today.graphql sample ===" << endl << endl;

    // clang-format off
    Schema schema({
        .typeDefs = readFile(SCHEMA_TODAY_PATH),
        .resolvers = {
            {"Query", Resolver{
                {"appointments", Resolver{
                    {"pageInfo", Resolver{
                        {"hasNextPage",    false},
                        {"hasPreviousPage",false}
                    }},
                    {"edges", vector<ValueResolver>{
                        Resolver{
                            {"cursor", "cursor:1"},
                            {"node", Resolver{
                                {"id",      "appt:1"},
                                {"subject", "Team standup"},
                                {"isNow",   true}
                            }}
                        },
                        Resolver{
                            {"cursor", "cursor:2"},
                            {"node", Resolver{
                                {"id",      "appt:2"},
                                {"subject", "1:1 with manager"},
                                {"isNow",   false}
                            }}
                        }
                    }}
                }},
                {"tasks", Resolver{
                    {"pageInfo", Resolver{
                        {"hasNextPage",    false},
                        {"hasPreviousPage",false}
                    }},
                    {"edges", vector<ValueResolver>{
                        Resolver{
                            {"cursor", "cursor:t1"},
                            {"node", Resolver{
                                {"id",         "task:1"},
                                {"title",      "Fix the build"},
                                {"isComplete", false}
                            }}
                        },
                        Resolver{
                            {"cursor", "cursor:t2"},
                            {"node", Resolver{
                                {"id",         "task:2"},
                                {"title",      "Write unit tests"},
                                {"isComplete", true}
                            }}
                        }
                    }}
                }}
            }}
        }
    });
    // clang-format on

    const string query = R"(
        query {
            appointments {
                pageInfo { hasNextPage hasPreviousPage }
                edges {
                    cursor
                    node { id subject isNow }
                }
            }
            tasks {
                pageInfo { hasNextPage }
                edges {
                    node { id title isComplete }
                }
            }
        }
    )";

    auto result = schema.Resolve({
        .query = query
    }).get();

    if (result.errors.has_value()) {
        for (const auto& e : result.errors.value())
            cerr << "Error: " << e.message << endl;
        return 1;
    }

    try {
        cout << json::parse(result.data.value()).dump(2) << endl;
    } catch (const json::parse_error& e) {
        cerr << "JSON parse error: " << e.what() << endl;
        cerr << "Raw: " << result.data.value() << endl;
        return 1;
    }

    return 0;
}

