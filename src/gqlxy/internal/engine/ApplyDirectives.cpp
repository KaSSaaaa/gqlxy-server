#include "ApplyDirectives.h"

#include <gqlxy/internal/ast/Directive.h>
#include <gqlxy/internal/engine/ResolveArguments.h>
#include <gqlxy/ResolverArgs.h>

using namespace std;

namespace gqlxy::internal {

optional<ValueResolver> ApplyDirectives(const vector<Directive>& directives,
                                        const Directives& allDirectives,
                                        const nlohmann::json& variables,
                                        const ValueResolver& value) {
    auto current = make_optional(value);

    for (const auto& [name, args] : directives) {
        auto& directive = allDirectives.at(name);
        current = directive(ResolverArgs({ .args = ResolveArguments(args, variables) }), *current);
        if (!current.has_value())
            break;
    }
    return current;
}

}