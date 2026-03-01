#include "schema_parser.h"

#include <ariane/internal/introspection/types/SchemaDefinition.h>
#include <ariane/internal/peg/first_node.h>
#include <ariane/internal/peg/transform_children.h>
#include <ariane/internal/utils/optional.h>
#include <graphqlservice/internal/Grammar.h>

using namespace std;
using namespace graphql;
using namespace tao;

namespace ariane::graphql::internal {

static optional<string> ParseDescription(const peg::ast_node& node) {
    return and_then(first_node<peg::description>(node), [](const auto& desc) {
        return or_else(and_then(first_node<peg::string_quote_character>(*desc), [](const auto& quoted) {
            return make_optional(quoted->string());
        }), [&desc]() {
            return and_then(first_node<peg::block_quote_content_lines>(*desc), [](const auto& blockQuoted) {
                return make_optional<string>(blockQuoted->unescaped_view());
            });
        });
    });
}

static optional<string> ParseDeprecationReason(const peg::ast_node& node) {
    return or_else(and_then(first_node<peg::string_quote_character>(node), [](const auto& q) {
        return make_optional(q->string());
    }), [&node]() {
        return and_then(first_node<peg::block_quote_content_lines>(node), [](const auto& q) {
            return make_optional<string>(q->unescaped_view());
        });
    });
}

static TypeRef ParseTypeRefFromNode(const peg::ast_node& node) {
    return or_else(and_then(first_node<peg::nonnull_type>(node), [](const auto& nnt) {
        return make_optional(ParseTypeRef(*nnt));
    }), [&node]() {
        return or_else(and_then(first_node<peg::list_type>(node), [](const auto& lt) {
            return make_optional(ParseTypeRef(*lt));
        }), [&node]() {
            return and_then(first_node<peg::named_type>(node), [](const auto& nt) {
                return make_optional(ParseTypeRef(*nt));
            });
        });
    }).value();
}

//TODO clean this
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

static optional<string> ParseDefaultValue(const peg::ast_node& node) {
    auto dv = first_node<peg::default_value>(node);
    if (!dv) return nullopt;

    const peg::ast_node& dvNode = *dv.value();
    if (find_node<peg::true_keyword>(dvNode))
        return "true";
    if (find_node<peg::false_keyword>(dvNode))
        return "false";
    if (auto v = find_node<peg::list_value>(dvNode))
        return (*v)->string();
    if (auto v = find_node<peg::string_value>(dvNode))
        return (*v)->string();
    if (auto v = find_node<peg::integer_value>(dvNode))
        return (*v)->string();
    if (auto v = find_node<peg::float_value>(dvNode))
        return (*v)->string();
    if (auto v = find_node<peg::enum_value>(dvNode))
        return (*v)->string();
    return nullopt;
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
                            return ParseDeprecationReason(*sv);
                        }), [reason]() {
                            return and_then(first_node<peg::input_value>(*reason), [](const auto* iv) {
                                return ParseDeprecationReason(*iv);
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
        .defaultValue = ParseDefaultValue(node)
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
        .type = or_else(and_then(or_else(first_node<peg::nonnull_type>(node), [&node]() {
            return or_else(first_node<peg::list_type>(node), [&node]() {
                return first_node<peg::named_type>(node);
            });
        }), [](const auto* typeNode) {
            return make_optional(ParseTypeRef(*typeNode));
        }), []() {
            return TypeRef::Named("Unknown");
        }).value(),
        .args = and_then(first_node<peg::arguments_definition>(node), [](const auto& argsNode) {
            return transform_children<peg::input_field_definition, InputValueDefinition>(*argsNode,
                [](const auto& child) {
                    return ParseInputValue(child);
                });
        }),
        .deprecation = ParseDeprecation(node)
    };
}

vector<FieldDefinition> ParseFields(const optional<peg::ast_node*>& node) {
    return and_then(node, [](auto* node) {
        return transform_children<peg::field_definition, FieldDefinition>(*node, [](const auto& node) {
            return ParseField(node);
        });
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
        .interfaces = transform_children<peg::interface_type, string>(node, [](const auto& child) {
            return child.string();
        })
    };
}

TypeDefinition ParseInterfaceType(const peg::ast_node& node) {
    return TypeDefinition{
        .kind = TypeKind::INTERFACE,
        .name = and_then(first_node<peg::interface_name>(node), [](auto* node) {
            return node->string();
        }),
        .description = ParseDescription(node),
        .fields = ParseFields(first_node<peg::fields_definition>(node))
    };
}

string ParseName(const peg::ast_node& node, const TypeKind& kind) {
    switch (kind) {
        case TypeKind::ENUM:
            return and_then(first_node<peg::enum_name>(node), [](const auto* n) {
                return n->string();
            });
        case TypeKind::SCALAR:
            return and_then(first_node<peg::scalar_name>(node), [](const auto* n) {
                return n->string();
            });
        case TypeKind::UNION:
            return and_then(first_node<peg::union_name>(node), [](const auto* n) {
                return n->string();
            });
        case TypeKind::INPUT_OBJECT:
            return and_then(first_node<peg::object_name>(node), [](const auto* n) {
                return n->string();
            });
        default:
            return "";
    }
}

TypeDefinition ParseType(const peg::ast_node& node, const TypeKind& kind) {
    return TypeDefinition {
        .kind = kind,
        .name = ParseName(node, kind),
        .description = ParseDescription(node),
        .unionTypes = transform_children<peg::union_type, string>(node, [](const auto& child) {
            return child.string();
        }),
        .enumValues = transform_children<peg::enum_value_definition, EnumValueDefinition>(node, [](const auto& child) {
            return ParseEnumValue(child);
        }),
        .inputFields = and_then(first_node<peg::input_fields_definition>(node), [](const auto& in) {
            return transform_children<peg::input_field_definition, InputValueDefinition>(*in, [](const auto& child) {
                return ParseInputValue(child);
            });
        })
    };
}

static DirectiveDefinition ParseDirective(const peg::ast_node& node) {
    return DirectiveDefinition {
        .name = and_then(first_node<peg::directive_name>(node), [](const auto* n) {
            return n->string();
        }),
        .description = ParseDescription(node),
        .locations = transform_children<peg::directive_location, DirectiveLocation>(node, [](const auto& locNode) {
            return *DirectiveLocation::_from_string_nothrow(locNode.string().c_str());
        }),
        .args = and_then(first_node<peg::arguments_definition>(node), [](const auto* args) {
            return transform_children<peg::input_field_definition, InputValueDefinition>(*args, [](const auto& child) {
                return ParseInputValue(child);
            });
        }),
        .isRepeatable = first_node<peg::repeatable_keyword>(node).has_value(),
    };
}

static unordered_map<string_view, std::function<TypeDefinition(const peg::ast_node&)>> typeParsers {
    {graphqlpeg::demangle<peg::object_type_definition>(), [](const auto& node) {
        return ParseObjectType(node);
    }},
    {graphqlpeg::demangle<peg::scalar_type_definition>(), [](const auto& node) {
        return ParseType(node, TypeKind::SCALAR);
    }},
    {graphqlpeg::demangle<peg::enum_type_definition>(), [](const auto& node) {
        return ParseType(node, TypeKind::ENUM);
    }},
    {graphqlpeg::demangle<peg::interface_type_definition>(), [](const auto& node) {
        return ParseInterfaceType(node);
    }},
    {graphqlpeg::demangle<peg::union_type_definition>(), [](const auto& node) {
        return ParseType(node, TypeKind::UNION);
    }},
    {graphqlpeg::demangle<peg::input_object_type_definition>(), [](const auto& node) {
        return ParseType(node, TypeKind::INPUT_OBJECT);
    }},
};

optional<TypeDefinition> ParseType(const peg::ast_node& node) {
    if (auto it = typeParsers.find(node.type); it != typeParsers.end()) {
        return it->second(node);
    }
    return nullopt;
}

void ParseSchemaDefinition(const shared_ptr<SchemaDefinition>& schemaDefinition, const unique_ptr<peg::ast_node>& node) {
    peg::for_each_child<peg::root_operation_definition>(*node, [schemaDefinition](const auto& operationDefinition) {
        auto operationType = and_then(first_node<peg::operation_type>(operationDefinition),
            [](const auto* operation) { return make_optional(operation->string()); }).value_or("");
        auto operationTypeName = and_then(first_node<peg::named_type>(operationDefinition),
            [](const auto* operation) { return make_optional(operation->string()); }).value_or("");

        if (operationType == "query")
            schemaDefinition->queryTypeName = operationTypeName;
        else if (operationType == "mutation")
            schemaDefinition->mutationTypeName = operationTypeName;
        else if (operationType == "subscription")
            schemaDefinition->subscriptionTypeName = operationTypeName;
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
                ParseSchemaDefinition(schemaDefinition, node);
            } else if (node->is_type<peg::directive_definition>()) {
                schemaDefinition->directives.push_back(ParseDirective(*node));
            } else {
                and_then(ParseType(*node), [schemaDefinition](const auto& typeDef) {
                    return schemaDefinition->types[typeDef.name] = typeDef;
                });
            }
        }

        for (const auto& [name, type] : schemaDefinition->types) {
            for (const auto& interface : type.interfaces) {
                schemaDefinition->types[interface].possibleTypes.push_back(name);
            }
        }

    } catch (const exception&) {
    }

    return schemaDefinition;
}

}
