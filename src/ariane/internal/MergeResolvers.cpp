#include "MergeResolvers.h"

using namespace std;

namespace ariane::graphql::internal {

void MergeResolvers(Resolver& left, const Resolver& right) {
    ranges::copy(right, inserter(left, left.end()));
}

}