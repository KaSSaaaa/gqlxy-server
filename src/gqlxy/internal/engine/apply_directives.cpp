#include "apply_directives.h"

#include <gqlxy/parser/ast/directive.h>
#include <gqlxy/internal/engine/resolve_arguments.h>
#include <gqlxy/resolver_args.h>

using namespace std;
using namespace gqlxy::parser;

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