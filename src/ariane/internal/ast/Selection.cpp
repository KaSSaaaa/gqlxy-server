#include "Selection.h"

#include <ariane/internal/ast/FragmentDefinition.h>
#include <ariane/internal/engine/ApplyDirectives.h>
#include <ariane/internal/utils/ranges.h>
#include <ariane/internal/utils/visit.h>

using namespace std;

namespace ariane::graphql::internal {

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