#pragma once

#include "environment_data.h"

namespace oled {
void begin(const environment::Snapshot& data);
// Apply the dimming mode chosen by the application.
void setDimmed(bool dimmed);
void update(const environment::Snapshot& data);
}  // namespace oled
