#include "Selection.h"

#include <ariane/internal/ast/FragmentDefinition.h>
#include <ariane/internal/engine/ApplyDirectives.h>
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
    if (concreteType && typeCondition.has_value() && typeCondition.value() != *concreteType)
        return {};
    return FlattenSelections(selectionSet, frags, directives, variables, concreteType);
}

std::vector<Field> FlattenSelections(const SelectionSet& ss,
                                     const Fragments& frags,
                                     const Directives& directives,
                                     const nlohmann::json& variables,
                                     const std::optional<std::string>& concreteType)  {
    vector<Field> fields;
    for (const auto& sel : ss.selections) {
        std::visit(overloaded{
           [&](const Field& f) { fields.push_back(f); },
           [&](const FragmentSpread& s) {
               if (!frags.contains(s.name))
                   return;
               auto& fragment = frags.at(s.name);
               auto nested = FragmentFields(fragment.selectionSet, s.directives, directives, variables, frags, fragment.typeCondition, concreteType);
               fields.insert(fields.end(), nested.begin(), nested.end());
           },
           [&](const InlineFragment& i) {
               auto nested = FragmentFields(*i.selectionSet, i.directives, directives, variables, frags, i.typeCondition, concreteType);
               fields.insert(fields.end(), nested.begin(), nested.end());
           },
        }, sel);
    }
    return fields;
}

}