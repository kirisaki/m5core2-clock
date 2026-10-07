#pragma once

#include <ctime>

namespace clock_service {
void begin();
void update();
bool localTime(tm& result);
}  // namespace clock_service
