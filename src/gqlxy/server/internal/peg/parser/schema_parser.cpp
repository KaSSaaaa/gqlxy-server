#include "schema_parser.h"

#include <gqlxy/server/internal/introspection/types/schema_definition.h>
#include <gqlxy/core/parser/peg/parser/parse_type_ref.h>
#include <gqlxy/core/parser/peg/parser/query/parse_directives.h>
#include <gqlxy/core/parser/peg/parser/query/parse_value.h>
#include <gqlxy/core/utils/optional.h>
#include <gqlxy/core/utils/peg/first_node.h>
#include <gqlxy/core/utils/peg/is_type.h>
#include <gqlxy/core/utils/ranges.h>
#include <graphqlservice/internal/Grammar.h>

using namespace std;
using namespace gqlxy::parser;
using namespace gqlxy::utils;
using namespace graphql;
using namespace tao;

namespace gqlxy::internal {

static optional<string> ParseQuotedString(const peg::ast_node& node) {
    return or_else(and_then(first_node<peg::string_quote_character>(node), [](const auto& q) {
        return make_optional(q->string());
    }), [&node]() {
        return and_then(first_node<peg::block_quote_content_lines>(node), [](const auto& q) {
            return make_optional<string>(q->unescaped_view());
        });
    });
}

static optional<string> ParseDescription(const peg::ast_node& node) {
    return and_then(first_node<peg::description>(node), [](const auto& desc) {
        return ParseQuotedString(*desc);
    });
}

static optional<string> ParseDefaultValue(const peg::ast_node& node) {
    return and_then(first_node<peg::default_value>(node), [](const auto* defaultValue) {
        return ParseValue(*defaultValue);
    });
}

static DeprecationInfo ParseDeprecation(const peg::ast_node& node) {
    return and_then(first_node<peg::directives>(node), [](const auto* directives) {
        return and_then(first_node<peg::directive>(*directives, [](const auto& directive) {
            return and_then(first_node<peg::directive_name>(directive), [](const auto* name) {
                return name->string() == "deprecated";
            });
        }), [](const auto* directive) {
            return DeprecationInfo{
                .isDeprecated = true,
                .deprecationReason = and_then(first_node<peg::arguments>(*directive), [](const auto* args) {
                    return and_then(first_node<peg::argument>(*args, [](const auto& arg) {
                        auto name = first_node<peg::argument_name>(arg);
                        return name.has_value() && (*name)->string() == "reason";
                    }), [](const auto* reason) {
                        return or_else(and_then(first_node<peg::string_value>(*reason), [](const auto* sv) {
                            return ParseQuotedString(*sv);
                        }), [reason]() {
                            return and_then(first_node<peg::input_value>(*reason), [](const auto* iv) {
                                return ParseQuotedString(*iv);
                            });
                        });
                    });
                })
            };
        });
    });
}

InputValueDefinition ParseInputValue(const peg::ast_node& node) {
    return InputValueDefinition {
        .name = and_then(first_node<peg::argument_name>(node), [](const auto& arg) {
            return make_optional(arg->string());
        }).value_or(""),
        .description = ParseDescription(node),
        .type = ParseTypeRefFromNode(node),
        .defaultValue = ParseDefaultValue(node),
        .deprecation = ParseDeprecation(node)
    };
}

EnumValueDefinition ParseEnumValue(const peg::ast_node& node) {
    return EnumValueDefinition{
        .name = and_then(first_node<peg::enum_value>(node), [](auto* node) {
            return node->string();
        }),
        .description = ParseDescription(node),
        .deprecation = ParseDeprecation(node)
    };
}

FieldDefinition ParseField(const peg::ast_node& node) {
    return FieldDefinition {
        .name = and_then(first_node<peg::field_name>(node), [](auto* node) {
            return node->string();
        }),
        .description = ParseDescription(node),
        .type = ParseTypeRefFromNode(node),
        .args = and_then(first_node<peg::arguments_definition>(node), [](const auto& argsNode) {
            return to_vector(argsNode->children
                | views::filter(is_type<peg::input_field_definition>())
                | views::transform([](const auto& node) {
                    return ParseInputValue(*node);
                }));
        }),
        .deprecation = ParseDeprecation(node)
    };
}

vector<FieldDefinition> ParseFields(const optional<peg::ast_node*>& node) {
    return and_then(node, [](auto* node) {
        return to_vector(node->children
            | views::filter(is_type<peg::field_definition>())
            | views::transform([](const auto& node) {
                return ParseField(*node);
            }));
    });
}

TypeDefinition ParseObjectType(const peg::ast_node& node) {
    return TypeDefinition{
        .kind = TypeKind::OBJECT,
        .name = and_then(first_node<peg::object_name>(node), [](auto* node) {
            return node->string();
        }),
        .description = ParseDescription(node),
        .fields = ParseFields(first_node<peg::fields_definition>(node)),
        .interfaces = to_vector(node.children
            | views::filter(is_type<peg::interface_type>())
            | views::transform([](const auto& child) {
                return child->string();
            })),
        .directives = ParseDirectives(node)
    };
}

TypeDefinition ParseInterfaceType(const peg::ast_node& node) {
    return TypeDefinition{
        .kind = TypeKind::INTERFACE,
        .name = and_then(first_node<peg::interface_name>(node), [](auto* node) {
            return node->string();
        }),
        .description = ParseDescription(node),
        .fields = ParseFields(first_node<peg::fields_definition>(node)),
        .directives = ParseDirectives(node)
    };
}

string ParseName(const peg::ast_node& node, const TypeKind& kind) {
    switch (kind) {
        case TypeKind::ENUM:
            return and_then(first_node<peg::enum_name>(node), [](const auto* n) { return n->string(); });
        case TypeKind::SCALAR:
            return and_then(first_node<peg::scalar_name>(node), [](const auto* n) { return n->string(); });
        case TypeKind::UNION:
            return and_then(first_node<peg::union_name>(node), [](const auto* n) { return n->string(); });
        case TypeKind::INPUT_OBJECT:
            return and_then(first_node<peg::object_name>(node), [](const auto* n) { return n->string(); });
        default:
            return "";
    }
}

TypeDefinition ParseType(const peg::ast_node& node, const TypeKind& kind) {
    return TypeDefinition {
        .kind = kind,
        .name = ParseName(node, kind),
        .description = ParseDescription(node),
        .unionTypes = to_vector(node.children
            | views::filter(is_type<peg::union_type>())
            | views::transform([](const auto& child) { return child->string(); })),
        .enumValues = to_vector(node.children
            | views::filter(is_type<peg::enum_value_definition>())
            | views::transform([](const auto& child) { return ParseEnumValue(*child); })),
        .inputFields = and_then(first_node<peg::input_fields_definition>(node), [](const auto& in) {
            return to_vector(in->children
                | views::filter(is_type<peg::input_field_definition>())
                | views::transform([](const auto& child) { return ParseInputValue(*child); }));
        })
    };
}

static DirectiveDefinition ParseDirective(const peg::ast_node& node) {
    return DirectiveDefinition {
        .name = and_then(first_node<peg::directive_name>(node), [](const auto* n) {
            return n->string();
        }),
        .description = ParseDescription(node),
        .locations = to_vector(node.children
            | views::filter(is_type<peg::directive_location>())
            | views::transform([](const auto& locNode) {
                return *DirectiveLocation::_from_string_nothrow(locNode->string().c_str());
            })),
        .args = and_then(first_node<peg::arguments_definition>(node), [](const auto* args) {
            return to_vector(args->children
                | views::filter(is_type<peg::input_field_definition>())
                | views::transform([](const auto& child) { return ParseInputValue(*child); }));
        }),
        .isRepeatable = first_node<peg::repeatable_keyword>(node).has_value(),
    };
}

static optional<TypeDefinition> ParseExtension(const peg::ast_node& node);

static unordered_map<string_view, function<TypeDefinition(const peg::ast_node&)>> typeParsers {
    {graphqlpeg::demangle<peg::object_type_definition>(), ParseObjectType},
    {graphqlpeg::demangle<peg::interface_type_definition>(), ParseInterfaceType},
    {graphqlpeg::demangle<peg::scalar_type_definition>(), [](const auto& node) { return ParseType(node, TypeKind::SCALAR); }},
    {graphqlpeg::demangle<peg::enum_type_definition>(), [](const auto& node) { return ParseType(node, TypeKind::ENUM); }},
    {graphqlpeg::demangle<peg::union_type_definition>(), [](const auto& node) { return ParseType(node, TypeKind::UNION); }},
    {graphqlpeg::demangle<peg::input_object_type_definition>(), [](const auto& node) { return ParseType(node, TypeKind::INPUT_OBJECT); }},
};

optional<TypeDefinition> ParseType(const peg::ast_node& node) {
    if (auto it = typeParsers.find(node.type); it != typeParsers.end()) {
        return it->second(node);
    }
    return nullopt;
}

static unordered_map<string_view, function<TypeDefinition(const peg::ast_node&)>> extensionParsers {
    {graphqlpeg::demangle<peg::object_type_extension>(), ParseObjectType},
    {graphqlpeg::demangle<peg::interface_type_extension>(), ParseInterfaceType},
    {graphqlpeg::demangle<peg::union_type_extension>(), [](const auto& n) { return ParseType(n, TypeKind::UNION); }},
    {graphqlpeg::demangle<peg::enum_type_extension>(), [](const auto& n) { return ParseType(n, TypeKind::ENUM); }},
    {graphqlpeg::demangle<peg::input_object_type_extension>(), [](const auto& n) { return ParseType(n, TypeKind::INPUT_OBJECT); }},
};

static optional<TypeDefinition> ParseExtension(const peg::ast_node& node) {
    if (auto it = extensionParsers.find(node.type); it != extensionParsers.end()) {
        return it->second(node);
    }
    return nullopt;
}

static void ApplyExtension(SchemaDefinition& schema, const TypeDefinition& ext) {
    auto it = schema.types.find(ext.name);
    if (it == schema.types.end()) return;
    auto& type = it->second;
    ranges::copy(ext.fields, back_inserter(type.fields));
    ranges::copy(ext.interfaces, back_inserter(type.interfaces));
    ranges::copy(ext.unionTypes, back_inserter(type.unionTypes));
    ranges::copy(ext.enumValues, back_inserter(type.enumValues));
    ranges::copy(ext.inputFields, back_inserter(type.inputFields));
}

static void ParseSchemaDefinition(const shared_ptr<SchemaDefinition>& schemaDefinition,
                                  const peg::ast_node& node) {
    peg::for_each_child<peg::root_operation_definition>(node, [schemaDefinition](const auto& operationDefinition) {
        auto operationType = and_then(first_node<peg::operation_type>(operationDefinition),
            [](const auto* operation) { return operation->string(); });
        auto operationTypeName = and_then(first_node<peg::named_type>(operationDefinition),
            [](const auto* operation) { return operation->string(); });

        if (operationType == "query") schemaDefinition->queryTypeName = operationTypeName;
        else if (operationType == "mutation") schemaDefinition->mutationTypeName = operationTypeName;
        else if (operationType == "subscription") schemaDefinition->subscriptionTypeName = operationTypeName;
    });
}

shared_ptr<SchemaDefinition> ParseSchemaDefinition(const string& typeDefs) {
    auto schemaDefinition = make_shared<SchemaDefinition>();

    try {
        auto ast = peg::parseSchemaString(typeDefs);

        if (!ast.root) {
            return schemaDefinition;
        }

        for (const auto& node : ast.root->children) {
            if (node->is_type<peg::schema_definition>()) {
                ParseSchemaDefinition(schemaDefinition, *node);
            } else if (node->is_type<peg::directive_definition>()) {
                schemaDefinition->directives.push_back(ParseDirective(*node));
            } else {
                and_then(ParseType(*node), [schemaDefinition](const auto& typeDef) {
                    return schemaDefinition->types[typeDef.name] = typeDef;
                });
            }
        }

        for (const auto& ext : ast.root->children
            | views::transform([](const auto& node) { return ParseExtension(*node); })
            | views::filter([](const auto& ext) { return ext.has_value(); })) {
            ApplyExtension(*schemaDefinition, ext.value());
        }

        for (const auto& [name, interface] : schemaDefinition->InterfacesPerType()) {
            schemaDefinition->types[interface].possibleTypes.push_back(name);
        }

    } catch (const exception&) {
    }

    return schemaDefinition;
}

}
