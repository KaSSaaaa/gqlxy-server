#include "JsonToValueResolver.h"

using namespace std;

namespace ariane::graphql::internal {

ValueResolver JsonToValueResolver(const nlohmann::json& j) {
    if (j.is_null())
        return monostate{};
    if (j.is_boolean())
        return j.get<bool>();
    if (j.is_number_integer())
        return j.get<int>();
    if (j.is_number_float())
        return j.get<double>();
    if (j.is_string())
        return j.get<string>();
    return monostate{};
}

}