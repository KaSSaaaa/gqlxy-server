#include <ariane/ResolverArgs.h>
#include <ariane/resolvers.h>
#include <ariane/schema.h>
#include <ariane/task.h>
#include <fstream>
#include <future>
#include <iostream>
#include <nlohmann/json.hpp>
#include <sstream>
#include <string>

using namespace std;
using namespace ariane::graphql;
using json = nlohmann::json;

static string readFile(const char* path) {
    ifstream f(path);
    if (!f) throw runtime_error(string("Cannot open schema file: ") + path);
    ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

int main() {
    cout << "=== Ariane — schema.today.graphql sample ===" << endl << endl;

    unordered_map<string, Resolver> nodes = {
        {"1", Resolver {
            {"id", "1"},
            {"title", "Ariane to the moon 🚀"},
            {"isComplete", false}
        }},
        {"2", Resolver {
            {"id", "2"},
            {"when", "2024-05-08T23:18:29.551Z"},
            {"subject", "First commit"},
            {"isNow", false},
            {"forceError", nullopt}
        }}
    };

    Schema schema({
        .typeDefs = readFile(SCHEMA_TODAY_PATH),
        .resolvers = {
            {"Query", Resolver {
                {"node", FunctionResolver([&nodes](const ResolverArgs& args) -> ValueResolver {
                    if (auto node = nodes.find(args.Args()["id"].get<string>()); node != nodes.end()) {
                        return node->second;
                    }
                    return nullopt;
                })}
            }},
            {"Node", Resolver {
                {"__resolveType", TypeResolver([](const Resolver& resolver) -> optional<string> {
                    if (resolver.contains("title"))
                        return "Task";

                    if (resolver.contains("when"))
                        return "Appointment";

                    return nullopt;
                })}
            }}
        }
    });

    auto result = schema.Resolve({
        .query = R"(
            query GetNodeById($id: ID!) {
                node(id: $id) {
                    __typename
                    id
                }
            }
        )",
        .variables = {
            {"id", "1"}
        }
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

