#include <ariane/internal/introspection/introspection.h>
#include <ariane/internal/introspection/types/Document.h>
#include <ariane/internal/parser/parser.h>
#include <ariane/internal/peg/first_node.h>
#include <ariane/internal/utils/visit.h>
#include <ariane/schema.h>
#include <ariane/task.h>
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

}

Schema::Schema(const SchemaOptions& options) : _resolvers(options.resolvers) {
    _document = ParseTypeDefs(options.typeDefs);

    if (options.allowIntrospection)
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
                            while (typeRef && (typeRef->kind._value == TypeRefKind::NON_NULL ||
                                              typeRef->kind._value == TypeRefKind::LIST)) {
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
                                      while (typeRef && (typeRef->kind._value == TypeRefKind::NON_NULL ||
                                                         typeRef->kind._value == TypeRefKind::LIST)) {
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
