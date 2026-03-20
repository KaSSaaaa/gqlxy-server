#include <ariane/schema.h>
#include <nlohmann/json.hpp>

#include <fstream>
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

static void writeFile(const char* path, const char* data) {
    ofstream f(path);
    if (!f) {
        throw runtime_error(string("Cannot open schema file: ") + path);
    }
    f << data;
}

int main() {
    cout << "=== Ariane — schema.today.graphql introspection ===" << endl << endl;

    Schema schema({
        .typeDefs  = readFile(SCHEMA_TODAY_PATH),
        .resolvers = {
            {"Query", Resolver{}}
        }
    });

    auto result = schema.Resolve({
        .query = readFile(INTROSPECTION_QUERY_PATH),
    }).get();

    if (result.errors.has_value()) {
        for (const auto& e : result.errors.value())
            cerr << "Error: " << e.message << endl;
        return 1;
    }

    auto __type = schema.Resolve({
        .query = R"(
            query GetType($typename: String!) {
                __type(name: $typename) {
                    kind
                    name
                }
            }
        )",
        .variables = {
            {"typename", "Node"}
        }
    }).get();

    if (result.errors.has_value()) {
        for (const auto& e : result.errors.value())
            cerr << "Error: " << e.message << endl;
        return 1;
    }

    if (__type.errors.has_value()) {
        for (const auto& e : __type.errors.value())
            cerr << "Error: " << e.message << endl;
        return 1;
    }

    try {
        auto jsonData = result.data.value().dump(2);
        auto jsonResult = json::parse(readFile(RESULT_PATH)).dump(2);
        writeFile(OUTPUT_PATH, jsonData.data());
        cout << jsonData << endl;
        cout << "========" << endl;
        cout << "Are results equal : " << boolalpha << (jsonData == jsonResult) << endl;
        cout << __type.data.value().dump(2) << endl;
    } catch (const json::parse_error& e) {
        cerr << "JSON parse error: " << e.what() << endl;
        cerr << "Raw: " << result.data.value() << endl;
        return 1;
    }

    return 0;
}

