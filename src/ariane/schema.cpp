#include <ariane/internal/introspection/introspection.h>
#include <ariane/internal/introspection/types/Document.h>
#include <ariane/internal/peg/first_node.h>
#include <ariane/internal/peg/transform_children.h>
#include <ariane/internal/utils/optional.h>
#include <ariane/schema.h>
#include <ariane/task.h>
#include <graphqlservice/GraphQLParse.h>
#include <graphqlservice/internal/Grammar.h>
#include <nlohmann/json.hpp>

using namespace std;
using namespace ariane::graphql;
using namespace ariane::graphql::internal;
using namespace graphql;

using FragmentMap = unordered_map<string, const peg::ast_node*>;

Task<nlohmann::json> ResolveValue(const ValueResolver& resolver,
                            const peg::ast_node* selectionSet,
                            const string& typeName,
                            const Document& doc,
                            const FragmentMap& fragments);

namespace {

template<class... Ts>
struct overloaded : Ts... { using Ts::operator()...; };

// Returns all field nodes in a selection set, recursively expanding fragment spreads.
vector<const peg::ast_node*> FlattenFields(const peg::ast_node* selectionSet,
                                            const FragmentMap& fragments) {
    if (!selectionSet)
        return {};

    vector<const peg::ast_node*> fields;

    for (const auto& child : selectionSet->children) {
        if (!child)
            continue;
        if (child->is_type<peg::field>()) {
            fields.push_back(child.get());
        } else if (child->is_type<peg::fragment_spread>()) {
            auto nameNode = first_node<peg::fragment_name>(*child);
            if (!nameNode)
                continue;
            auto it = fragments.find((*nameNode)->string());
            if (it == fragments.end())
                continue;
            auto nested = FlattenFields(it->second, fragments);
            fields.insert(fields.end(), nested.begin(), nested.end());
        }
    }

    return fields;
}

TypeRef ParseTypeRef(const peg::ast_node& node) {
    if (node.is_type<peg::nonnull_type>()) {
        auto innerNode = first_node<peg::list_type>(node);
        if (!innerNode) {
            innerNode = first_node<peg::named_type>(node);
        }
        if (innerNode) {
            return TypeRef::NonNull(ParseTypeRef(**innerNode));
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

InputValueDefinition ParseInputValue(const peg::ast_node& node) {
    InputValueDefinition inputValue;

    auto nameNode = first_node<peg::argument_name>(node);
    if (nameNode) {
        inputValue.name = (*nameNode)->string();
    }

    auto typeNameNode = first_node<peg::type_name>(node);
    if (typeNameNode) {
        inputValue.type = TypeRef::Named((*typeNameNode)->string());
    } else {
        inputValue.type = TypeRef::Named("Unknown");
    }

    return inputValue;
}

EnumValueDefinition ParseEnumValue(const peg::ast_node& node) {
    return EnumValueDefinition {
        .name = and_then(first_node<peg::enum_value>(node), [](auto* node) {
            return make_optional(node->string());
        }).value_or("")
    };
}

TypeDefinition ParseType(const peg::ast_node& node, const TypeKind& kind) {
    TypeDefinition typeDef;
    typeDef.kind = kind;

    if (kind._value == TypeKind::SCALAR) {
        typeDef.name = and_then(first_node<peg::scalar_name>(node), [](auto* n) -> optional<string> {
            return n->string();
        }).value_or("");
    } else if (kind._value == TypeKind::ENUM) {
        typeDef.name = and_then(first_node<peg::enum_name>(node), [](auto* n) -> optional<string> {
            return n->string();
        }).value_or("");

        for_each_child<peg::enum_value_definition>(node, [&typeDef](const auto& child) {
            typeDef.enumValues.push_back(ParseEnumValue(child));
        });
    } else if (kind._value == TypeKind::UNION) {
        typeDef.name = and_then(first_node<peg::union_name>(node), [](auto* n) -> optional<string> {
            return n->string();
        }).value_or("");

        for_each_child<peg::union_type>(node, [&typeDef](const auto& child) {
            typeDef.unionTypes.push_back(child.string());
        });
    }

    return typeDef;
}

FieldDefinition ParseField(const peg::ast_node& node) {
    FieldDefinition field;

    field.name = and_then(first_node<peg::field_name>(node), [](auto* node) -> optional<string> {
        return node->string();
    }).value_or("");

    auto typeNode = first_node<peg::nonnull_type>(node);
    if (!typeNode) {
        typeNode = first_node<peg::list_type>(node);
    }
    if (!typeNode) {
        typeNode = first_node<peg::named_type>(node);
    }

    if (typeNode) {
        field.type = ParseTypeRef(**typeNode);
    } else {
        field.type = TypeRef::Named("Unknown");
    }

    auto argsNode = first_node<peg::arguments_definition>(node);
    if (argsNode) {
        field.args = transform_children<peg::input_field_definition, InputValueDefinition>(
            *argsNode.value(),
            [](const auto& child) {
                return ParseInputValue(child);
            }
        );
    }

    return field;
}

vector<FieldDefinition> ParseFields(const optional<peg::ast_node*>& node) {
    return and_then(node, [](auto* node) -> optional<vector<FieldDefinition>> {
        return transform_children<peg::field_definition, FieldDefinition>(*node, [](const auto& node) {
            return ParseField(node);
        });
    }).value_or(vector<FieldDefinition>{});
}

TypeDefinition ParseObjectType(const peg::ast_node& node) {
    return TypeDefinition {
        .kind = TypeKind::OBJECT,
        .name = and_then(first_node<peg::object_name>(node), [](auto* node) -> optional<string> {
            return node->string();
        }).value_or(""),
        .fields = ParseFields(first_node<peg::fields_definition>(node)),
        .interfaces = transform_children<peg::interface_type, string>(node, [](const auto& child) {
            return child.string();
        })
    };
}

TypeDefinition ParseInterfaceType(const peg::ast_node& node) {
    return TypeDefinition {
        .kind = TypeKind::INTERFACE,
        .name = and_then(first_node<peg::interface_name>(node), [](auto* node) -> optional<string> {
            return node->string();
        }).value_or(""),
        .fields = ParseFields(first_node<peg::fields_definition>(node))
    };
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

}

Schema::Schema(const SchemaOptions& options) : _resolvers(options.resolvers) {
    _document = ParseTypeDefs(options.typeDefs);
    InjectIntrospectionResolvers();
}

void Schema::InjectIntrospectionResolvers() {
    if (!_resolvers.contains("Query")) {
        _resolvers["Query"] = Resolver{};
    }

    auto& queryResolver = _resolvers.at("Query");
    if (!holds_alternative<Resolver>(queryResolver)) {
        return;
    }

    auto& queryFields = get<Resolver>(queryResolver);

    queryFields["__schema"] = [doc = _document]() {
        return ValueResolver(CreateSchemaResolver(*doc));
    };

    queryFields["__typename"] = string("Query");
}

Task<ResolveResult> Schema::Resolve(const string& query, const unordered_map<string, string>& variables) {
    ResolveResult result;

    try {
        auto ast = peg::parseString(query);

        if (!ast.root) {
            result.errors = nlohmann::json{
                {"errors", {
                    {{"message", "Failed to parse query"}}
                }}
            }.dump();
            co_return result;
        }

        nlohmann::json data = nlohmann::json::object();

        // Collect all fragment definitions from the document
        FragmentMap fragments;
        for (const auto& child : ast.root->children) {
            if (!child || !child->is_type<peg::fragment_definition>())
                continue;
            auto nameNode = first_node<peg::fragment_name>(*child);
            auto ssNode   = first_node<peg::selection_set>(*child);
            if (nameNode && ssNode)
                fragments[(*nameNode)->string()] = (*ssNode);
        }

        for (const auto& child : ast.root->children) {
            if (!child || !child->is_type<peg::operation_definition>()) {
                continue;
            }

            auto operationTypeNode = first_node<peg::operation_type>(*child);
            if (!operationTypeNode.has_value()) {
                continue;
            }

            string operationType = (*operationTypeNode)->string();
            if (operationType == "query") {
                operationType = "Query";
            } else if (operationType == "mutation") {
                operationType = "Mutation";
            } else if (operationType == "subscription") {
                operationType = "Subscription";
            }

            if (!_resolvers.contains(operationType)) {
                continue;
            }

            auto& typeResolver = _resolvers.at(operationType);
            if (!holds_alternative<Resolver>(typeResolver)) {
                continue;
            }

            auto& fieldResolvers = get<Resolver>(typeResolver);

            auto selectionSetNode = first_node<peg::selection_set>(*child);
            if (!selectionSetNode.has_value()) {
                continue;
            }

            for (const auto* selection : FlattenFields((*selectionSetNode), fragments)) {
                auto fieldNameNode = first_node<peg::field_name>(*selection);
                if (!fieldNameNode.has_value()) {
                    continue;
                }

                const string fieldName = (*fieldNameNode)->string();

                if (!fieldResolvers.contains(fieldName)) {
                    continue;
                }

                string fieldTypeName;
                if (_document->types.contains(operationType)) {
                    for (const auto& f : _document->types.at(operationType).fields) {
                        if (f.name == fieldName) {
                            const TypeRef* typeRef = &f.type;
                            while (typeRef && (typeRef->kind._value == TypeRefKind::NonNull ||
                                              typeRef->kind._value == TypeRefKind::List)) {
                                typeRef = typeRef->ofType ? typeRef->ofType.get() : nullptr;
                            }
                            if (typeRef && !typeRef->name.empty()) {
                                fieldTypeName = typeRef->name;
                            }
                            break;
                        }
                    }
                }

                auto nestedSelectionSet = first_node<peg::selection_set>(*selection);
                data[fieldName] = co_await ResolveValue(
                     fieldResolvers.at(fieldName),
                     nestedSelectionSet.has_value() ? nestedSelectionSet.value() : nullptr,
                     fieldTypeName.empty() ? operationType : fieldTypeName,
                     *_document,
                     fragments);
            }
        }

        result.data = data.dump();

    } catch (const exception& e) {
        result.errors = nlohmann::json{
            {"errors", {
                {{"message", e.what()}}
            }}
        }.dump();
    }

    co_return result;
}

Task<nlohmann::json> ResolveValue(const ValueResolver& resolver,
                                  const peg::ast_node* selectionSet,
                                  const string& typeName,
                                  const Document& doc,
                                  const FragmentMap& fragments) {
    co_return co_await std::visit(
         overloaded{
              [](int v) -> Task<nlohmann::json> { co_return v; },
              [](uint64_t v) -> Task<nlohmann::json> { co_return v; },
              [](double v) -> Task<nlohmann::json> { co_return v; },
              [](float v) -> Task<nlohmann::json> { co_return v; },
              [](bool v) -> Task<nlohmann::json> { co_return v; },
              [](const string& v) -> Task<nlohmann::json> { co_return v; },
              [](monostate) -> Task<nlohmann::json> { co_return nullptr; },
              [&](const Resolver& nestedResolver) -> Task<nlohmann::json> {
                  auto obj = nlohmann::json::object();
                  if (selectionSet) {
                      for (const auto* selection : FlattenFields(selectionSet, fragments)) {
                          auto fieldNameNode = first_node<peg::field_name>(*selection);
                          if (!fieldNameNode.has_value()) {
                              continue;
                          }

                          auto fieldName = (*fieldNameNode)->string();

                          if (fieldName == "__typename") {
                              obj["__typename"] = typeName;
                              continue;
                          }

                          if (!nestedResolver.contains(fieldName)) {
                              continue;
                          }
                          string fieldTypeName;
                          if (!typeName.empty() && doc.types.contains(typeName)) {
                              for (const auto& f : doc.types.at(typeName).fields) {
                                  if (f.name == fieldName) {
                                      const TypeRef* typeRef = &f.type;
                                      while (typeRef && (typeRef->kind._value == TypeRefKind::NonNull ||
                                                         typeRef->kind._value == TypeRefKind::List)) {
                                          typeRef = typeRef->ofType ? typeRef->ofType.get() : nullptr;
                                      }
                                      if (typeRef && !typeRef->name.empty()) {
                                          fieldTypeName = typeRef->name;
                                      }
                                      break;
                                  }
                              }
                          }

                          auto nestedSelectionSet = first_node<peg::selection_set>(*selection);
                          obj[fieldName] = co_await ResolveValue(
                               nestedResolver.at(fieldName),
                               nestedSelectionSet.has_value() ? nestedSelectionSet.value() : nullptr,
                               fieldTypeName,
                               doc,
                               fragments);
                      }
                  } else {
                      for (const auto& [key, value] : nestedResolver) {
                          obj[key] = co_await ResolveValue(value, nullptr, "", doc, fragments);
                      }
                  }

                  co_return obj;
              },
              [&](const vector<ValueResolver>& vec) -> Task<nlohmann::json> {
                  nlohmann::json arr = nlohmann::json::array();
                  for (const auto& item : vec) {
                      arr.push_back(co_await ResolveValue(item, selectionSet, typeName, doc, fragments));
                  }
                  co_return arr;
              },
              [&](const FunctionResolver& func) -> Task<nlohmann::json> {
                  co_return co_await ResolveValue(func(), selectionSet, typeName, doc, fragments);
              },
              [&](const AsyncFunctionResolver& func) -> Task<nlohmann::json> {
                  co_return co_await ResolveValue(func().get(), selectionSet, typeName, doc, fragments);
              },
              [&](const CoroutineResolver& func) -> Task<nlohmann::json> {
                  co_return co_await ResolveValue(co_await func(), selectionSet, typeName, doc, fragments);
              },
              [&](const CallbackResolver& func) -> Task<nlohmann::json> {
                  ValueResolver callbackResult;
                  func([&callbackResult](const auto& res) { callbackResult = res; });
                  co_return co_await ResolveValue(callbackResult, selectionSet, typeName, doc, fragments);
              },
         },
         resolver);
}

shared_ptr<Document> Schema::ParseTypeDefs(const string& typeDefs) {
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
                    if (op == "query")        doc->queryTypeName = name;
                    else if (op == "mutation")     doc->mutationTypeName = name;
                    else if (op == "subscription") doc->subscriptionTypeName = name;
                }
                continue;
            }

            auto typeDef = ParseType(*node);
            if (typeDef.has_value()) {
                doc->types[typeDef.value().name] = typeDef.value();
            }
        }

    } catch (const exception&) {
    }

    return doc;
}
