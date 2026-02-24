#include "parser.h"

#include <ariane/internal/introspection/types/Document.h>
#include <ariane/internal/peg/first_node.h>
#include <ariane/internal/peg/transform_children.h>
#include <ariane/internal/utils/optional.h>
#include <graphqlservice/internal/Grammar.h>

using namespace std;
using namespace graphql;

namespace ariane::graphql::internal {

static string trimBlockString(string_view s) {
    vector<string> lines;
    size_t start = 0;
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '\n') {
            lines.push_back(string(s.substr(start, i - start)));
            start = i + 1;
        }
    }
    lines.push_back(string(s.substr(start)));

    auto isBlank = [](const string& line) {
        return line.find_first_not_of(" \t\r") == string::npos;
    };
    while (!lines.empty() && isBlank(lines.front())) lines.erase(lines.begin());
    while (!lines.empty() && isBlank(lines.back())) lines.pop_back();

    string result;
    for (size_t i = 0; i < lines.size(); ++i) {
        if (i > 0) result += '\n';
        result += lines[i];
    }
    return result;
}

static optional<string> ParseDescription(const peg::ast_node& node) {
    auto desc = first_node<peg::description>(node);
    if (!desc) return nullopt;
    const auto view = (*desc)->unescaped_view();
    if (view.empty()) return nullopt;
    return trimBlockString(view);
}

TypeRef ParseTypeRef(const peg::ast_node& node) {
    if (node.is_type<peg::nonnull_type>()) {
        auto innerNode = first_node<peg::list_type>(node);
        if (!innerNode) {
            innerNode = first_node<peg::named_type>(node);
        }
        if (innerNode) {
            return TypeRef::NonNull(ParseTypeRef(*innerNode.value()));
        }
        return TypeRef::NonNull(TypeRef::Named("Unknown"));
    }

    if (node.is_type<peg::list_type>()) {
        auto innerNode = first_node<peg::nonnull_type>(node);
        if (!innerNode) {
            innerNode = first_node<peg::list_type>(node);
        }
        if (!innerNode) {
            innerNode = first_node<peg::named_type>(node);
        }
        if (innerNode) {
            return TypeRef::List(ParseTypeRef(**innerNode));
        }
        return TypeRef::List(TypeRef::Named("Unknown"));
    }

    if (node.is_type<peg::named_type>()) {
        return TypeRef::Named(node.string());
    }

    return TypeRef::Named("Unknown");
}

static TypeRef ParseTypeRefFromNode(const peg::ast_node& node) {
    if (auto t = first_node<peg::nonnull_type>(node))
        return ParseTypeRef(**t);
    if (auto t = first_node<peg::list_type>(node))
        return ParseTypeRef(**t);
    if (auto t = first_node<peg::named_type>(node))
        return ParseTypeRef(**t);
    return TypeRef::Named("Unknown");
}

static optional<string> ParseDefaultValue(const peg::ast_node& node) {
    auto dv = first_node<peg::default_value>(node);
    if (!dv) return nullopt;
    const peg::ast_node& dvNode = **dv;
    if (auto v = find_node<peg::true_keyword>(dvNode))   return string("true");
    if (auto v = find_node<peg::false_keyword>(dvNode))  return string("false");
    if (auto v = find_node<peg::list_value>(dvNode))     return string((*v)->string());
    if (auto v = find_node<peg::string_value>(dvNode))   return string((*v)->string());
    if (auto v = find_node<peg::integer_value>(dvNode))  return string((*v)->string());
    if (auto v = find_node<peg::float_value>(dvNode))    return string((*v)->string());
    if (auto v = find_node<peg::enum_value>(dvNode))     return string((*v)->string());
    return nullopt;
}

static DeprecationInfo ParseDeprecation(const peg::ast_node& node) {
    auto directivesNode = first_node<peg::directives>(node);
    if (!directivesNode) return {};

    DeprecationInfo info;
    for_each_child<peg::directive>(**directivesNode, [&info](const peg::ast_node& dir) {
        auto nameNode = first_node<peg::directive_name>(dir);
        if (!nameNode || (*nameNode)->string() != "deprecated")
            return;

        info.isDeprecated = true;
        auto argsNode = first_node<peg::arguments>(dir);
        if (!argsNode) return;

        for_each_child<peg::argument>(**argsNode, [&info](const peg::ast_node& arg) {
            auto argName = first_node<peg::argument_name>(arg);
            if (!argName || (*argName)->string() != "reason") return;

            // Try string_value directly (intermediate nodes may be transparent)
            auto sv = first_node<peg::string_value>(arg);
            if (sv) {
                const auto view = (*sv)->unescaped_view();
                if (!view.empty())
                    info.deprecationReason = trimBlockString(view);
                return;
            }
            auto iv = first_node<peg::input_value>(arg);
            if (!iv) return;
            const auto view = (*iv)->unescaped_view();
            if (!view.empty())
                info.deprecationReason = trimBlockString(view);
        });
    });
    return info;
}

InputValueDefinition ParseInputValue(const peg::ast_node& node) {
    auto deprecation = ParseDeprecation(node);
    return InputValueDefinition {
        .name = and_then(first_node<peg::argument_name>(node), [](auto* node) -> optional<string> {
            return node->string();
        }).value_or(""),
        .description = ParseDescription(node),
        .type = ParseTypeRefFromNode(node),
        .defaultValue = ParseDefaultValue(node)
    };
}

EnumValueDefinition ParseEnumValue(const peg::ast_node& node) {
    auto deprecation = ParseDeprecation(node);
    return EnumValueDefinition{
        .name = and_then(first_node<peg::enum_value>(node), [](auto* node) {
            return make_optional(node->string());
        }).value_or(""),
        .description = ParseDescription(node),
        .isDeprecated = deprecation.isDeprecated,
        .deprecationReason = deprecation.deprecationReason
    };
}

FieldDefinition ParseField(const peg::ast_node& node) {
    auto typeNode = first_node<peg::nonnull_type>(node);
    if (!typeNode) {
        typeNode = first_node<peg::list_type>(node);
    }
    if (!typeNode) {
        typeNode = first_node<peg::named_type>(node);
    }

    optional<TypeRef> type;
    if (typeNode) {
        type = ParseTypeRef(*typeNode.value());
    }

    auto argsNode = first_node<peg::arguments_definition>(node);
    auto deprecation = ParseDeprecation(node);

    return FieldDefinition {
        .name = and_then(first_node<peg::field_name>(node), [](auto* node) -> optional<string> {
            return node->string();
        }).value_or(""),
        .description = ParseDescription(node),
        .type = type.value_or(TypeRef::Named("Unknown")),
        .args = argsNode.has_value()
            ? transform_children<peg::input_field_definition, InputValueDefinition>(*argsNode.value(),
                [](const auto& child) {
                    return ParseInputValue(child);
                })
            : vector<InputValueDefinition>(),
        .isDeprecated = deprecation.isDeprecated,
        .deprecationReason = deprecation.deprecationReason
    };
}

vector<FieldDefinition> ParseFields(const optional<peg::ast_node*>& node) {
    return and_then(node, [](auto* node) -> optional<vector<FieldDefinition>> {
        return transform_children<peg::field_definition, FieldDefinition>(*node,
            [](const auto& node) {
                return ParseField(node);
            });
    }).value_or(vector<FieldDefinition>{});
}

TypeDefinition ParseObjectType(const peg::ast_node& node) {
    return TypeDefinition{
        .kind = TypeKind::OBJECT,
        .name = and_then(first_node<peg::object_name>(node), [](auto* node) -> optional<string> {
            return node->string();
        }).value_or(""),
        .description = ParseDescription(node),
        .fields = ParseFields(first_node<peg::fields_definition>(node)),
        .interfaces = transform_children<peg::interface_type, string>(node, [](const auto& child) {
            return child.string();
        })
    };
}

TypeDefinition ParseInterfaceType(const peg::ast_node& node) {
    return TypeDefinition{
        .kind = TypeKind::INTERFACE,
        .name = and_then(first_node<peg::interface_name>(node), [](auto* node) -> optional<string> {
            return node->string();
        }).value_or(""),
        .description = ParseDescription(node),
        .fields = ParseFields(first_node<peg::fields_definition>(node))
    };
}

TypeDefinition ParseType(const peg::ast_node& node, const TypeKind& kind) {
    TypeDefinition typeDef;
    typeDef.kind = kind;

    if (kind._value == TypeKind::SCALAR) {
        typeDef.name = and_then(first_node<peg::scalar_name>(node), [](auto* n) -> optional<string> {
            return n->string();
        }).value_or("");
        typeDef.description = ParseDescription(node);
    } else if (kind._value == TypeKind::ENUM) {
        typeDef.name = and_then(first_node<peg::enum_name>(node), [](auto* n) -> optional<string> {
            return n->string();
        }).value_or("");
        typeDef.description = ParseDescription(node);
        for_each_child<peg::enum_value_definition>(node, [&typeDef](const auto& child) {
            typeDef.enumValues.push_back(ParseEnumValue(child));
        });
    } else if (kind._value == TypeKind::UNION) {
        typeDef.name = and_then(first_node<peg::union_name>(node), [](auto* n) -> optional<string> {
            return n->string();
        }).value_or("");
        typeDef.description = ParseDescription(node);
        for_each_child<peg::union_type>(node, [&typeDef](const auto& child) {
            typeDef.unionTypes.push_back(child.string());
        });
    } else if (kind._value == TypeKind::INPUT_OBJECT) {
        typeDef.name = and_then(first_node<peg::object_name>(node), [](auto* n) -> optional<string> {
            return n->string();
        }).value_or("");
        typeDef.description = ParseDescription(node);

        auto fieldsNode = first_node<peg::input_fields_definition>(node);
        if (fieldsNode) {
            typeDef.inputFields = transform_children<peg::input_field_definition, InputValueDefinition>(
                **fieldsNode, [](const auto& child) {
                    return ParseInputValue(child);
                });
        }
    }

    return typeDef;
}

static DirectiveDefinition ParseDirective(const peg::ast_node& node) {
    DirectiveDefinition directive;
    directive.description = ParseDescription(node);
    directive.name = and_then(first_node<peg::directive_name>(node), [](auto* n) -> optional<string> {
        return n->string();
    }).value_or("");
    directive.isRepeatable = first_node<peg::repeatable_keyword>(node).has_value();

    auto argsNode = first_node<peg::arguments_definition>(node);
    if (argsNode) {
        directive.args = transform_children<peg::input_field_definition, InputValueDefinition>(
            **argsNode, [](const auto& child) {
                return ParseInputValue(child);
            });
    }

    for_each_child<peg::directive_location>(node, [&directive](const peg::ast_node& locNode) {
        auto locStr = locNode.string();
        auto loc = DirectiveLocation::_from_string_nothrow(locStr.c_str());
        if (loc) directive.locations.push_back(*loc);
    });

    return directive;
}

optional<TypeDefinition> ParseType(const peg::ast_node& node) {
    if (node.is_type<peg::object_type_definition>()) {
        return ParseObjectType(node);
    }
    if (node.is_type<peg::scalar_type_definition>()) {
        return ParseType(node, TypeKind::SCALAR);
    }
    if (node.is_type<peg::enum_type_definition>()) {
        return ParseType(node, TypeKind::ENUM);
    }
    if (node.is_type<peg::interface_type_definition>()) {
        return ParseInterfaceType(node);
    }
    if (node.is_type<peg::union_type_definition>()) {
        return ParseType(node, TypeKind::UNION);
    }
    if (node.is_type<peg::input_object_type_definition>()) {
        return ParseType(node, TypeKind::INPUT_OBJECT);
    }
    return nullopt;
}

shared_ptr<Document> ParseTypeDefs(const string& typeDefs) {
    auto doc = make_shared<Document>();

    try {
        auto ast = peg::parseSchemaString(typeDefs);

        if (!ast.root) {
            return doc;
        }

        for (const auto& node : ast.root->children) {
            if (!node)
                continue;

            if (node->is_type<peg::schema_definition>()) {
                for (const auto& opDef : node->children) {
                    if (!opDef || !opDef->is_type<peg::root_operation_definition>())
                        continue;
                    auto opType = first_node<peg::operation_type>(*opDef);
                    auto namedType = first_node<peg::named_type>(*opDef);
                    if (!opType || !namedType)
                        continue;
                    const string op = (*opType)->string();
                    const string name = (*namedType)->string();
                    if (op == "query")
                        doc->queryTypeName = name;
                    else if (op == "mutation")
                        doc->mutationTypeName = name;
                    else if (op == "subscription")
                        doc->subscriptionTypeName = name;
                }
                continue;
            }

            if (node->is_type<peg::directive_definition>()) {
                doc->directives.push_back(ParseDirective(*node));
                continue;
            }

            auto typeDef = ParseType(*node);
            if (typeDef.has_value()) {
                const auto& name = typeDef.value().name;
                doc->typeOrder.push_back(name);
                doc->types[name] = typeDef.value();
            }
        }

        // Build possibleTypes for INTERFACE types (reverse mapping, in SDL order)
        for (const auto& name : doc->typeOrder) {
            const auto& type = doc->types.at(name);
            if (type.kind._value != TypeKind::OBJECT) continue;
            for (const auto& iface : type.interfaces) {
                if (doc->types.contains(iface)) {
                    doc->types[iface].possibleTypes.push_back(name);
                }
            }
        }

    } catch (const exception&) {
    }

    return doc;
}

}
