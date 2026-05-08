#pragma once

#include <gqlxy/server/subscription.h>
#include "resolve.h"

namespace gqlxy::internal {

SubscriptionHandle Subscribe(const ResolveQueryArgs& args);

}
