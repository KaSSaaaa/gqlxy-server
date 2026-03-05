#include "ApplyDirectives.h"

#include <ariane/internal/ast/Directive.h>
#include <ariane/internal/engine/ResolveArguments.h>

using namespace std;

namespace ariane::graphql::internal {

optional<ValueResolver> ApplyDirectives(const vector<Directive>& directives,
                                        const Directives& allDirectives,
                                        const nlohmann::json& variables,
                                        const ValueResolver& value) {
    auto current = make_optional(value);

    for (const auto& [name, args] : directives) {
        auto& directive = allDirectives.at(name);
        current = directive(ResolverArgs{ .args = ResolveArguments(args, variables) }, *current);
        if (!current.has_value())
            break;
    }
    return current;
}

}