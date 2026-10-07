#pragma once

#include "environment_data.h"

namespace network {
enum class Status { Unconfigured, Connecting, Connected, Error };

void begin();
Status status();
bool consumeTimeSync();
bool receive(environment::Snapshot& snapshot);
}  // namespace network
