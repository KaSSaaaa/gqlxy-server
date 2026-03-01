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

    unordered_map<string, Resolver> nodes = {
        {"1", Resolver {
            {"id", "1"}
        }},
        {"2", Resolver {
            {"id", "2"}
        }}
    };

    Schema schema({
        .typeDefs = readFile(SCHEMA_TODAY_PATH),
        .resolvers = {
            {"Query", Resolver {
                {"node", FunctionResolver([&nodes](const ResolverArgs& args) -> ValueResolver {
                    if (auto node = nodes.find(args.args["id"].get<string>()); node != nodes.end()) {
                        return node->second;
                    }
                    return nullopt;
                })}
            }}
        }
    });

    auto result = schema.Resolve({
        .query = R"(
            query GetNodeById($id: ID!) {
                node(id: $id) {
                    id
                }
            }
        )",
        .variables = {
            {"id", "1"}
        }
    }).get();

    if (result.errors.has_value()) {
        cerr << "Errors: " << result.errors.value() << endl;
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

