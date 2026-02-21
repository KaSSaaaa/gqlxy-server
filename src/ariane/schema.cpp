#include <ariane/introspection.h>
#include <ariane/optional.h>
#include <ariane/schema.h>
#include <graphqlservice/GraphQLParse.h>
#include <graphqlservice/internal/Grammar.h>
#include <set>
#include <sstream>

using namespace ariane::graphql;
using namespace graphql::peg;
using namespace std;

void ResolveValue(std::ostringstream& stream,
                  const ValueResolver& resolver,
                  const ast_node* selectionSet,
                  const std::string& typeName,
                  const Document* doc);

TypeRef TypeRef::Named(const std::string& typeName) {
    TypeRef ref;
    ref.kind = TypeRefKind::NamedType;
    ref.name = typeName;
    ref.ofType = nullptr;
    return ref;
}

TypeRef TypeRef::NonNull(TypeRef inner) {
    TypeRef ref;
    ref.kind = TypeRefKind::NonNull;
    ref.name = "";
    ref.ofType = std::make_unique<TypeRef>(std::move(inner));
    return ref;
}

TypeRef TypeRef::List(TypeRef inner) {
    TypeRef ref;
    ref.kind = TypeRefKind::List;
    ref.name = "";
    ref.ofType = std::make_unique<TypeRef>(std::move(inner));
    return ref;
}

namespace {

template <typename T>
optional<ast_node*> first_node(const ast_node& node) {
    for (const auto& child : node.children) {
        if (child && child->is_type<T>()) {
            return child.get();
        }
    }
    return nullopt;
}

template <typename TNode, typename TResult>
std::vector<TResult> transform_children(const ast_node& node, const function<TResult(const ast_node&)>& transform) {
    std::vector<TResult> result;
    for_each_child<TNode>(node, [transform, &result](const ast_node& child) { result.emplace_back(transform(child)); });
    return result;
}

TypeRef ParseTypeRef(const ast_node& node) {
    if (node.is_type<nonnull_type>()) {
        auto innerNode = first_node<list_type>(node);
        if (!innerNode) {
            innerNode = first_node<named_type>(node);
        }
        if (innerNode) {
            return TypeRef::NonNull(ParseTypeRef(**innerNode));
        }
        return TypeRef::NonNull(TypeRef::Named("Unknown"));
    }

    if (node.is_type<list_type>()) {
        auto innerNode = first_node<nonnull_type>(node);
        if (!innerNode) {
            innerNode = first_node<list_type>(node);
        }
        if (!innerNode) {
            innerNode = first_node<named_type>(node);
        }
        if (innerNode) {
            return TypeRef::List(ParseTypeRef(**innerNode));
        }
        return TypeRef::List(TypeRef::Named("Unknown"));
    }

    if (node.is_type<named_type>()) {
        return TypeRef::Named(node.string());
    }

    return TypeRef::Named("Unknown");
}

InputValueDefinition ParseInputValue(const ast_node& node) {
    InputValueDefinition inputValue;

    auto nameNode = first_node<argument_name>(node);
    if (nameNode) {
        inputValue.name = (*nameNode)->string();
    }

    auto typeNameNode = first_node<type_name>(node);
    if (typeNameNode) {
        inputValue.type = TypeRef::Named((*typeNameNode)->string());
    } else {
        inputValue.type = TypeRef::Named("Unknown");
    }

    return inputValue;
}

EnumValueDefinition ParseEnumValue(const ast_node& node) {
    EnumValueDefinition enumValue;

    auto valueNode = first_node<enum_value>(node);
    if (valueNode) {
        enumValue.name = (*valueNode)->string();
    }

    return enumValue;
}

TypeDefinition ParseType(const ast_node& node, const TypeKind& kind) {
    TypeDefinition typeDef;
    typeDef.kind = kind;

    if (kind._value == TypeKind::SCALAR) {
        typeDef.name = and_then(first_node<scalar_name>(node), [](auto* n) -> optional<std::string> {
                           return n->string();
                       }).value_or("");
    } else if (kind._value == TypeKind::ENUM) {
        typeDef.name = and_then(first_node<enum_name>(node), [](auto* n) -> optional<std::string> {
                           return n->string();
                       }).value_or("");

        for_each_child<enum_value_definition>(
             node, [&typeDef](const ast_node& child) { typeDef.enumValues.push_back(ParseEnumValue(child)); });
    } else if (kind._value == TypeKind::UNION) {
        typeDef.name = and_then(first_node<union_name>(node), [](auto* n) -> optional<std::string> {
                           return n->string();
                       }).value_or("");

        for_each_child<union_type>(node,
                                   [&typeDef](const ast_node& child) { typeDef.unionTypes.push_back(child.string()); });
    }

    return typeDef;
}

FieldDefinition ParseField(const ast_node& node) {
    FieldDefinition field;

    field.name = and_then(first_node<field_name>(node), [](auto* node) -> std::optional<std::string> {
                     return node->string();
                 }).value_or("");

    auto typeNode = first_node<nonnull_type>(node);
    if (!typeNode) {
        typeNode = first_node<list_type>(node);
    }
    if (!typeNode) {
        typeNode = first_node<named_type>(node);
    }

    if (typeNode) {
        field.type = ParseTypeRef(**typeNode);
    } else {
        field.type = TypeRef::Named("Unknown");
    }

    auto argsNode = first_node<arguments_definition>(node);
    if (argsNode) {
        field.args = transform_children<input_field_definition, InputValueDefinition>(
             **argsNode, [](const ast_node& child) { return ParseInputValue(child); });
    }

    return field;
}

vector<FieldDefinition> ParseFields(const optional<ast_node*>& node) {
    return and_then(node,
                    [](auto* node) -> optional<vector<FieldDefinition>> {
                        return transform_children<field_definition, FieldDefinition>(
                             *node, [](const ast_node& node) { return ParseField(node); });
                    })
         .value_or(vector<FieldDefinition>{});
}

TypeDefinition ParseObjectType(const ast_node& node) {
    TypeDefinition typeDef;
    typeDef.kind = TypeKind::OBJECT;
    typeDef.name = and_then(first_node<object_name>(node), [](auto* node) -> optional<std::string> {
                       return node->string();
                   }).value_or("");
    typeDef.fields = ParseFields(first_node<fields_definition>(node));

    for_each_child<interface_type>(node,
                                   [&typeDef](const ast_node& child) { typeDef.interfaces.push_back(child.string()); });

    return typeDef;
}

TypeDefinition ParseInterfaceType(const ast_node& node) {
    TypeDefinition typeDef;
    typeDef.kind = TypeKind::INTERFACE;
    typeDef.name = and_then(first_node<interface_name>(node), [](auto* node) -> optional<std::string> {
                       return node->string();
                   }).value_or("");
    typeDef.fields = ParseFields(first_node<fields_definition>(node));

    return typeDef;
}

optional<TypeDefinition> ParseType(const ast_node& node) {
    if (node.is_type<object_type_definition>()) {
        return ParseObjectType(node);
    }
    if (node.is_type<scalar_type_definition>()) {
        return ParseType(node, TypeKind::SCALAR);
    }
    if (node.is_type<enum_type_definition>()) {
        return ParseType(node, TypeKind::ENUM);
    }
    if (node.is_type<interface_type_definition>()) {
        return ParseInterfaceType(node);
    }
    if (node.is_type<union_type_definition>()) {
        return ParseType(node, TypeKind::UNION);
    }
    if (node.is_type<input_object_type_definition>()) {
        return ParseType(node, TypeKind::INPUT_OBJECT);
    }
    return nullopt;
}

}

Schema::Schema(const SchemaOptions& options) : resolvers_(options.resolvers) {
    document_ = ParseTypeDefs(options.typeDefs);
    InjectIntrospectionResolvers();
}

void Schema::InjectIntrospectionResolvers() {
    if (resolvers_.find("Query") == resolvers_.end()) {
        resolvers_["Query"] = Resolver{};
    }

    auto& queryResolver = resolvers_.at("Query");
    if (!std::holds_alternative<Resolver>(queryResolver)) {
        return;
    }

    auto& queryFields = std::get<Resolver>(queryResolver);

    queryFields["__schema"] = [doc = document_]() { return ValueResolver(CreateSchemaResolver(*doc)); };

    queryFields["__typename"] = std::string("Query");
}

ResolveResult Schema::Resolve(const std::string& query, const std::unordered_map<std::string, std::string>& variables) {
    ResolveResult result;

    try {
        auto ast = parseString(query);

        if (!ast.root) {
            result.errors = R"({"errors":[{"message":"Failed to parse query"}]})";
            return result;
        }

        std::ostringstream dataStream;
        dataStream << "{";

        bool firstField = true;

        for (const auto& child : ast.root->children) {
            if (!child || !child->is_type<operation_definition>()) {
                continue;
            }

            auto operationTypeNode = first_node<operation_type>(*child);
            if (!operationTypeNode.has_value()) {
                continue;
            }

            std::string operationType = (*operationTypeNode)->string();
            if (operationType == "query") {
                operationType = "Query";
            } else if (operationType == "mutation") {
                operationType = "Mutation";
            } else if (operationType == "subscription") {
                operationType = "Subscription";
            }

            if (resolvers_.find(operationType) == resolvers_.end()) {
                continue;
            }

            auto& typeResolver = resolvers_.at(operationType);
            if (!std::holds_alternative<Resolver>(typeResolver)) {
                continue;
            }

            auto& fieldResolvers = std::get<Resolver>(typeResolver);

            auto selectionSetNode = first_node<selection_set>(*child);
            if (!selectionSetNode.has_value()) {
                continue;
            }

            for (const auto& selection : (*selectionSetNode)->children) {
                if (!selection || !selection->is_type<field>()) {
                    continue;
                }

                auto fieldNameNode = first_node<field_name>(*selection);
                if (!fieldNameNode.has_value()) {
                    continue;
                }

                std::string fieldName = (*fieldNameNode)->string();

                if (fieldResolvers.find(fieldName) == fieldResolvers.end()) {
                    continue;
                }

                if (!firstField) {
                    dataStream << ",";
                }
                firstField = false;

                dataStream << "\"" << fieldName << "\":";

                auto& fieldResolver = fieldResolvers.at(fieldName);

                std::string fieldTypeName = "";
                if (document_->types.find(operationType) != document_->types.end()) {
                    const auto& typeDef = document_->types.at(operationType);
                    for (const auto& field : typeDef.fields) {
                        if (field.name == fieldName) {
                            const TypeRef* typeRef = &field.type;
                            while (typeRef &&
                                   (typeRef->kind._value == TypeRefKind::NonNull || typeRef->kind._value == TypeRefKind::List)) {
                                if (typeRef->ofType) {
                                    typeRef = typeRef->ofType.get();
                                } else {
                                    break;
                                }
                            }
                            if (typeRef && !typeRef->name.empty()) {
                                fieldTypeName = typeRef->name;
                            }
                            break;
                        }
                    }
                }

                auto nestedSelectionSet = first_node<selection_set>(*selection);
                ResolveValue(dataStream, fieldResolver,
                             nestedSelectionSet.has_value() ? nestedSelectionSet.value() : nullptr,
                             fieldTypeName.empty() ? operationType : fieldTypeName, document_.get());
            }
        }

        dataStream << "}";
        result.data = dataStream.str();

    } catch (const std::exception& e) {
        result.errors = std::string(R"({"errors":[{"message":")") + e.what() + R"("}]})";
    }

    return result;
}

void ResolveValue(std::ostringstream& stream,
                  const ValueResolver& resolver,
                  const ast_node* selectionSet,
                  const std::string& typeName,
                  const Document* doc) {
    if (std::holds_alternative<int>(resolver)) {
        stream << std::get<int>(resolver);
    } else if (std::holds_alternative<uint64_t>(resolver)) {
        stream << std::get<uint64_t>(resolver);
    } else if (std::holds_alternative<double>(resolver)) {
        stream << std::get<double>(resolver);
    } else if (std::holds_alternative<float>(resolver)) {
        stream << std::get<float>(resolver);
    } else if (std::holds_alternative<bool>(resolver)) {
        stream << (std::get<bool>(resolver) ? "true" : "false");
    } else if (std::holds_alternative<std::string>(resolver)) {
        stream << "\"" << std::get<std::string>(resolver) << "\"";
    } else if (std::holds_alternative<std::monostate>(resolver)) {
        stream << "null";
    } else if (std::holds_alternative<Resolver>(resolver)) {
        auto& nestedResolver = std::get<Resolver>(resolver);
        stream << "{";

        if (selectionSet) {
            bool first = true;
            for (const auto& selection : selectionSet->children) {
                if (!selection || !selection->is_type<field>()) {
                    continue;
                }

                auto fieldNameNode = first_node<field_name>(*selection);
                if (!fieldNameNode.has_value()) {
                    continue;
                }

                std::string fieldName = (*fieldNameNode)->string();

                if (fieldName == "__typename") {
                    if (!first)
                        stream << ",";
                    first = false;
                    stream << "\"__typename\":\"" << typeName << "\"";
                    continue;
                }

                if (nestedResolver.find(fieldName) == nestedResolver.end()) {
                    continue;
                }

                if (!first)
                    stream << ",";
                first = false;
                stream << "\"" << fieldName << "\":";

                std::string fieldTypeName = "";
                if (doc && !typeName.empty() && doc->types.find(typeName) != doc->types.end()) {
                    const auto& typeDef = doc->types.at(typeName);
                    for (const auto& field : typeDef.fields) {
                        if (field.name == fieldName) {
                            const TypeRef* typeRef = &field.type;
                            while (typeRef &&
                                   (typeRef->kind._value == TypeRefKind::NonNull || typeRef->kind._value == TypeRefKind::List)) {
                                if (typeRef->ofType) {
                                    typeRef = typeRef->ofType.get();
                                } else {
                                    break;
                                }
                            }
                            if (typeRef && !typeRef->name.empty()) {
                                fieldTypeName = typeRef->name;
                            }
                            break;
                        }
                    }
                }

                auto nestedSelectionSet = first_node<selection_set>(*selection);
                ResolveValue(stream, nestedResolver.at(fieldName),
                             nestedSelectionSet.has_value() ? nestedSelectionSet.value() : nullptr, fieldTypeName, doc);
            }
        } else {
            bool first = true;
            for (const auto& [key, value] : nestedResolver) {
                if (!first)
                    stream << ",";
                first = false;
                stream << "\"" << key << "\":";
                ResolveValue(stream, value, nullptr, "", doc);
            }
        }

        stream << "}";
    } else if (std::holds_alternative<std::vector<ValueResolver>>(resolver)) {
        auto& vec = std::get<std::vector<ValueResolver>>(resolver);
        stream << "[";
        bool first = true;
        for (const auto& item : vec) {
            if (!first)
                stream << ",";
            first = false;
            ResolveValue(stream, item, selectionSet, typeName, doc);
        }
        stream << "]";
    } else if (std::holds_alternative<FunctionResolver>(resolver)) {
        auto& func = std::get<FunctionResolver>(resolver);
        auto result = func();
        ResolveValue(stream, result, selectionSet, typeName, doc);
    } else if (std::holds_alternative<AsyncFunctionResolver>(resolver)) {
        auto& func = std::get<AsyncFunctionResolver>(resolver);
        auto future = func();
        auto result = future.get();
        ResolveValue(stream, result, selectionSet, typeName, doc);
    } else if (std::holds_alternative<CoroutineResolver>(resolver)) {
        auto& func = std::get<CoroutineResolver>(resolver);
        auto task = func();
        auto result = task.get();
        ResolveValue(stream, result, selectionSet, typeName, doc);
    } else if (std::holds_alternative<CallbackResolver>(resolver)) {
        auto& func = std::get<CallbackResolver>(resolver);
        ValueResolver callbackResult;
        func([&callbackResult](const ValueResolver& res) { callbackResult = res; });
        ResolveValue(stream, callbackResult, selectionSet, typeName, doc);
    } else {
        stream << "null";
    }
}

shared_ptr<Document> Schema::ParseTypeDefs(const std::string& typeDefs) {
    auto doc = make_shared<Document>();

    try {
        auto ast = parseSchemaString(typeDefs);

        if (!ast.root) {
            return doc;
        }

        for (const auto& node : ast.root->children) {
            if (!node)
                continue;

            auto typeDef = ParseType(*node);

            if (typeDef.has_value()) {
                doc->types[typeDef.value().name] = std::move(typeDef.value());
            }
        }

    } catch (const exception&) {
    }

    return doc;
}
