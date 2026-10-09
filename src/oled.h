#pragma once

#include "environment_data.h"

namespace oled {
void begin();
void setData(const environment::Snapshot& snapshot);
void update();
}  // namespace oled
