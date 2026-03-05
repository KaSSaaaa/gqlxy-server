#include "ResolveArguments.h"

#include <ariane/internal/ast/Argument.h>

using namespace std;
using namespace nlohmann;

namespace ariane::graphql::internal {

json ResolveArguments(const std::vector<Argument>& args, const json& variables) {
    return accumulate(args.begin(), args.end(), json::object(), [&](auto obj, const auto& variable) {
        auto [name, value] = variable;
        obj[name] = value.starts_with("$") ? variables[value.substr(1)] : json::parse(value);
        return obj;
    });
}

}
