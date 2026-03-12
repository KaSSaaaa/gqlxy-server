#pragma once

#include <ariane/subscription.h>
#include "resolve.h"

namespace ariane::graphql::internal {

SubscriptionHandle Subscribe(const ResolveQueryArgs& args);

}
