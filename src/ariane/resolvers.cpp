#include <ariane/resolvers.h>

using namespace std;

namespace ariane::graphql {

void MergeResolvers(Resolver& left, const Resolver& right) {
    ranges::copy(right, inserter(left, left.end()));
}

}
