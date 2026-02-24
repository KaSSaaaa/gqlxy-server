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
        .resolvers = {{"Query", Resolver{{"__typename", "Query"}}}}
    });

    const string query = readFile(INTROSPECTION_QUERY_PATH);

    auto result = schema.Resolve(query).get();

    if (!result.errors.empty()) {
        cerr << "Errors: " << result.errors << endl;
        return 1;
    }

    try {
        auto jsonData = json::parse(result.data).dump(2);
        auto jsonResult = json::parse(readFile(RESULT_PATH)).dump(2);
        writeFile(OUTPUT_PATH, jsonData.data());
        cout << jsonData << endl;
        cout << "========" << endl;
        cout << "Are results equal : " << std::boolalpha << (jsonData == jsonResult) << endl;
    } catch (const json::parse_error& e) {
        cerr << "JSON parse error: " << e.what() << endl;
        cerr << "Raw: " << result.data << endl;
        return 1;
    }

    return 0;
}

