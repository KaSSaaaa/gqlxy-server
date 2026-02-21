#pragma once

#include <ariane/resolvers.h>
#include <ariane/schema.h>

namespace ariane::graphql {

Resolver CreateSchemaResolver(const Document& schema);
Resolver CreateTypeResolver(const TypeDefinition& type);
Resolver CreateFieldResolver(const FieldDefinition& field);
Resolver CreateInputValueResolver(const InputValueDefinition& input);
Resolver CreateEnumValueResolver(const EnumValueDefinition& enumValue);
Resolver CreateTypeRefResolver(const TypeRef& typeRef);
Resolver CreateDirectiveResolver(const DirectiveDefinition& directive);

}
