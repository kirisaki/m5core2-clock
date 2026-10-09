#pragma once

#include "app_config.h"
#include "environment_data.h"

namespace environment {
// Availability is separate: callers can distinguish missing values from old ones.
// Unsigned subtraction handles millis() rollover; widen before adding API age.
inline bool sensorStale(const Snapshot& data, uint32_t nowMs) {
  return data.sensorError ||
         static_cast<uint64_t>(data.sensor.ageMs) +
             static_cast<uint32_t>(nowMs - data.sensorReceivedMs) >= config::kSensorStaleMs;
}

inline bool weatherStale(const Snapshot& data, uint32_t nowMs) {
  return data.weatherError ||
         static_cast<uint32_t>(nowMs - data.weatherReceivedMs) >= config::kWeatherStaleMs;
}
}  // namespace environment
