#include <ariane/results.h>
#include <ariane/internal/utils/ranges.h>

using namespace std;
using namespace nlohmann;
using namespace ariane::graphql::internal;

namespace ariane::graphql {

json Serialize(const ResolveResult& result) {
    json r;
    if (result.data.has_value()) r["data"] = result.data.value();
    if (result.errors.has_value()) {
        auto err = json::array();
        for (const auto& [message, path, locations] : result.errors.value()) {
            err.push_back({
                {"message", message},
                {"path", path},
                {"location", to_vector(locations | views::transform([](const auto& loc) -> json {
                    return {{"line", loc.line}, {"column", loc.column}};
                }))}
            });
        }
        r["errors"] = err;
    }
    return r;
}

}