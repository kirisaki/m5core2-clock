#pragma once

#include "environment_data.h"

namespace ambient {
enum class RainState { Unknown, None, Starting, Ongoing };
struct Rain {
  RainState state = RainState::Unknown;
  time_t change = 0;
  int probability = -1;
  bool complete = false;  // Complete hourly coverage of the next 24 hours.
};
struct Sun {
  time_t time = 0;
  bool rising = false;
};
enum class Mood { NoData, Cold, Hot, Humid, Dry, Happy, Sleepy };
Rain rain(const environment::Weather& weather, time_t now);
Sun nextSun(const environment::Weather& weather, time_t now);
// Approximate mean lunar age, not an astronomical ephemeris.
constexpr double kLunarMonth = 29.530588;
double moonAge(time_t now);
Mood mood(const environment::Snapshot& data, uint32_t nowMs, bool night);
}  // namespace ambient
