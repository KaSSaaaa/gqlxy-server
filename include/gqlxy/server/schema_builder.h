#pragma once

#include <boost/pfr.hpp>
#include <gqlxy/server/reflexion.h>
#include <gqlxy/server/schema.h>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace gqlxy {

// Builds a Schema from code-first GQLReflectable types (see reflexion.h), mirroring HotChocolate's
// SchemaBuilder: types/interfaces/unions are derived from C++ structs, root Query/Mutation/
// Subscription fields and any hand-written SDL (e.g. the root operation types themselves) are added
// explicitly.
class SchemaBuilder {
public:
    template<typename T>
        requires GQLReflectable<T>
    SchemaBuilder& AddType() {
        RegisterType<T>();
        return *this;
    }

    template<typename... Ts>
        requires(sizeof...(Ts) > 1) && (GQLReflectable<Ts> && ...)
    SchemaBuilder& AddUnion(const std::string& name) {
        (RegisterType<Ts>(), ...);
        _unionDefs.push_back("union " + name + " = " + JoinUnionMembers<Ts...>() + "\n");
        _resolvers[name] = Resolver {{"__resolveType", MakeDiscriminatorTypeResolver()}};
        return *this;
    }

    SchemaBuilder& AddTypeDefs(const std::string& sdl);
    SchemaBuilder& AddQuery(const std::string& fieldName, ValueResolver resolver);
    SchemaBuilder& AddMutation(const std::string& fieldName, ValueResolver resolver);
    SchemaBuilder& AddSubscription(const std::string& fieldName, ValueResolver resolver);

    Schema Build(SchemaOptions options = {}) const;

private:
    template<typename T>
    void RegisterType() {
        if (!_registered.insert(T::GQLName).second) return;
        _typeOrder.push_back(T::GQLName);
        _fieldsSdl[T::GQLName] = internal::SdlFieldsFor<T>(std::make_index_sequence<boost::pfr::tuple_size_v<T>>());
        _implementsClause[T::GQLName] = internal::ImplementsClauseFor(typename GQLImplements<T>::Interfaces {});
        RegisterInterfacesFrom(static_cast<typename GQLImplements<T>::Interfaces*>(nullptr));
    }

    template<typename... Is>
    void RegisterInterfacesFrom(std::tuple<Is...>*) {
        (RegisterInterface<Is>(), ...);
    }

    template<typename I>
    void RegisterInterface() {
        _interfaceNames.insert(I::GQLName);
        RegisterType<I>();
    }

    template<typename... Ts>
    static std::string JoinUnionMembers() {
        std::string joined;
        ((joined += (joined.empty() ? "" : " | ") + std::string(Ts::GQLName)), ...);
        return joined;
    }

    static TypeResolver MakeDiscriminatorTypeResolver();

    SchemaBuilder& AddToRoot(const std::string& rootName, const std::string& fieldName, ValueResolver resolver);
    std::string BuildTypeDefs() const;
    Resolver MergeResolvers(const Resolver& base) const;

    std::vector<std::string> _typeOrder;
    std::unordered_set<std::string> _registered;
    std::unordered_set<std::string> _interfaceNames;
    std::unordered_map<std::string, std::string> _fieldsSdl;
    std::unordered_map<std::string, std::string> _implementsClause;
    std::vector<std::string> _unionDefs;
    std::string _rawTypeDefs;
    Resolver _resolvers;
};

}
