#pragma once

#include "environment_data.h"

namespace oled {
void begin(const environment::Snapshot& data);
void update(const environment::Snapshot& data);
}  // namespace oled
