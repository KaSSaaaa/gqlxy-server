#pragma once

#include <gqlxy/server/definitions/type_definition.h>
#include <gqlxy/core/utils/ranges.h>

namespace gqlxy::internal {

static const std::vector BuiltInScalars = {
    TypeDefinition {
        .kind = TypeKind::SCALAR,
        .name = "String",
        .description = "The `String` scalar type represents textual data, represented as UTF-8 character sequences. The String type is most often used by GraphQL to represent free-form human-readable text."
    },
    TypeDefinition{
        .kind = TypeKind::SCALAR,
        .name = "Int",
        .description = "The `Int` scalar type represents non-fractional signed whole numeric values. Int can represent values between -(2^31) and 2^31 - 1."
    },
    TypeDefinition{
        .kind = TypeKind::SCALAR,
        .name = "Boolean",
        .description = "The `Boolean` scalar type represents `true` or `false`."
    },
    TypeDefinition{
        .kind = TypeKind::SCALAR,
        .name = "Float",
        .description = "The `Float` scalar type represents signed double-precision fractional values as specified by [IEEE 754](https://en.wikipedia.org/wiki/IEEE_floating_point)."
    },
    TypeDefinition{
        .kind = TypeKind::SCALAR,
        .name = "ID",
        .description = "The `ID` scalar type represents a unique identifier, often used to refetch an object or as key for a cache. The ID type appears in a JSON response as a String; however, it is not intended to be human-readable. When expected as an input type, any string (such as `\"4\"`) or integer (such as `4`) input value will be accepted as an ID."
    }
};

static auto BuiltinScalarsMap = utils::to_map(BuiltInScalars | std::views::transform([](const auto& typeDef) {
    return make_pair(typeDef.name, typeDef);
}));

}