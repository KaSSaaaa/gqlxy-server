#include <ariane/pubsub.h>
#include <ariane/resolvers.h>
#include <ariane/schema.h>
#include <ariane/subscription.h>
#include <nlohmann/json.hpp>

#include <chrono>
#include <future>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <thread>

using namespace std;
using namespace ariane::graphql;
using json = nlohmann::json;

static string currentDatetime() {
    auto now = chrono::system_clock::now();
    time_t t = chrono::system_clock::to_time_t(now);
    ostringstream ss;
    ss << put_time(localtime(&t), "%Y-%m-%dT%H:%M:%S");
    return ss.str();
}

int main() {
    cout << "=== Ariane — datetime subscription sample ===" << endl << endl;

    PubSub pubsub;
    Schema schema({
        .typeDefs = R"(
            type Query  { _unused: String }
            type Subscription { currentDatetime: String }
        )",
        .resolvers = {
            {"Subscription", Resolver{
                {"currentDatetime", SubscriptionResolver{[&pubsub](const ResolverArgs&) {
                    return pubsub.AsyncIterator({"TICK"});
                }}}
            }}
        }
    });

    auto handle = schema.Subscribe({
        .query = "subscription { currentDatetime }"
    });

    // Publish a datetime tick every second, 5 times
    auto publisher = async(launch::async, [&pubsub] {
        while (true) {
            this_thread::sleep_for(chrono::seconds(1));
            pubsub.Publish("TICK", currentDatetime());
        }
    });

    int i = 0;
    while (auto result = handle.Next()) {
        if (!result.has_value()) break;

        if (result->errors.has_value()) {
            for (const auto& e : *result->errors)
                cerr << "Error: " << e.message << endl;
            break;
        }

        auto data = result->data.value();
        cout << "tick " << ++i << ": " << data["currentDatetime"] << endl;
    }

    handle.Cancel();
    publisher.get();

    return 0;
}
