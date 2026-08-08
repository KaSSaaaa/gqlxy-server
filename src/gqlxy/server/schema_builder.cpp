#include <gqlxy/server/schema_builder.h>

using namespace std;
using namespace gqlxy;

SchemaBuilder& SchemaBuilder::AddTypeDefs(const string& sdl) {
    _rawTypeDefs += sdl + "\n";
    return *this;
}

SchemaBuilder& SchemaBuilder::AddQuery(const string& fieldName, ValueResolver resolver) {
    return AddToRoot("Query", fieldName, std::move(resolver));
}

SchemaBuilder& SchemaBuilder::AddMutation(const string& fieldName, ValueResolver resolver) {
    return AddToRoot("Mutation", fieldName, std::move(resolver));
}

SchemaBuilder& SchemaBuilder::AddSubscription(const string& fieldName, ValueResolver resolver) {
    return AddToRoot("Subscription", fieldName, std::move(resolver));
}

SchemaBuilder& SchemaBuilder::AddToRoot(const string& rootName, const string& fieldName, ValueResolver resolver) {
    auto it = _resolvers.find(rootName);
    if (it == _resolvers.end()) _resolvers[rootName] = Resolver {{fieldName, std::move(resolver)}};
    else it->second.As<Resolver>()[fieldName] = std::move(resolver);
    return *this;
}

TypeResolver SchemaBuilder::MakeDiscriminatorTypeResolver() {
    return TypeResolver {[](const Resolver& current) -> optional<string> {
        auto it = current.find(GQLTypeNameKey);
        if (it == current.end()) return nullopt;
        return it->second.AsIf<string>();
    }};
}

string SchemaBuilder::BuildTypeDefs() const {
    string typeDefs = _rawTypeDefs;
    for (const auto& name : _typeOrder) {
        const bool isInterface = _interfaceNames.contains(name);
        typeDefs += (isInterface ? "interface " : "type ") + name;
        if (!isInterface) typeDefs += _implementsClause.at(name);
        typeDefs += " {\n" + _fieldsSdl.at(name) + "}\n";
    }
    for (const auto& unionDef : _unionDefs)
        typeDefs += unionDef;
    return typeDefs;
}

Resolver SchemaBuilder::MergeResolvers(const Resolver& base) const {
    Resolver merged = base;
    for (const auto& [name, value] : _resolvers) {
        auto existing = merged.find(name);
        if (existing == merged.end() || !existing->second.Is<Resolver>() || !value.Is<Resolver>()) {
            merged[name] = value;
            continue;
        }
        for (const auto& [fieldName, fieldValue] : value.As<Resolver>())
            existing->second.As<Resolver>()[fieldName] = fieldValue;
    }
    return merged;
}

Schema SchemaBuilder::Build(SchemaOptions options) const {
    options.typeDefs = BuildTypeDefs() + "\n" + options.typeDefs;
    options.resolvers = MergeResolvers(options.resolvers);
    for (const auto& name : _interfaceNames)
        if (!options.resolvers.contains(name))
            options.resolvers[name] = Resolver {{"__resolveType", MakeDiscriminatorTypeResolver()}};
    return Schema(options);
}
