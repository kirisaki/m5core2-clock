#pragma once

#include "environment_data.h"

class U8G2;

namespace oled_renderer {
struct FrameTime {
  time_t utc = 0;
  tm local{};
  uint32_t uptimeMs = 0;
  bool valid = false;
};

// Draw into a cleared buffer. Hardware initialization, scheduling, and transfer
// belong to oled.cpp; no clock or device access occurs during rendering.
void dashboard(U8G2& display, const environment::Snapshot& data, const FrameTime& frame);
void pinTest(U8G2& display, const FrameTime& frame, const char* label);
}  // namespace oled_renderer
