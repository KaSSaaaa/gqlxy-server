#pragma once

#include <gqlxy/subscription.h>
#include "resolve.h"

namespace gqlxy::internal {

SubscriptionHandle Subscribe(const ResolveQueryArgs& args);

}
