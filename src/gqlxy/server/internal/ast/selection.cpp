#include "selection.h"

#include <gqlxy/server/internal/engine/apply_directives.h>
#include <gqlxy/core/parser/ast/fragment_definition.h>
#include <gqlxy/core/parser/ast/fragments.h>
#include <gqlxy/core/utils/ranges.h>
#include <gqlxy/core/utils/visit.h>

using namespace std;
using namespace gqlxy::parser;
using namespace gqlxy::utils;

namespace gqlxy::internal {

vector<Field> FragmentFields(const SelectionSet& selectionSet,
                             const vector<Directive>& fieldDirectives,
                             const Directives& directives,
                             const nlohmann::json& variables,
                             const Fragments& frags,
                             const optional<string>& typeCondition,
                             const optional<string>& concreteType) {
    if (!ApplyDirectives(fieldDirectives, directives, variables, monostate{}).has_value())
        return {};
    if (concreteType && typeCondition && *typeCondition != *concreteType)
        return {};
    return FlattenSelections(selectionSet, frags, directives, variables, concreteType);
}

vector<Field> FlattenSelections(const SelectionSet& ss,
                                const Fragments& frags,
                                const Directives& directives,
                                const nlohmann::json& variables,
                                const optional<string>& concreteType) {
    return flat_map(ss.selections, [&](const auto& sel) {
        return visit(overloaded{
           [&](const Field& f) -> vector<Field> { return { f }; },
           [&](const FragmentSpread& s) -> vector<Field> {
               if (!frags.contains(s.name))
                   return {};
               auto& fragment = frags.at(s.name);
               return FragmentFields(fragment.selectionSet, s.directives, directives, variables, frags, fragment.typeCondition, concreteType);
           },
           [&](const InlineFragment& i) {
               return FragmentFields(*i.selectionSet, i.directives, directives, variables, frags, i.typeCondition, concreteType);
           },
        }, sel);
    });
}

}