#pragma once

#include <cstddef>
#include <cstdint>
#include <ctime>

namespace environment {
constexpr size_t kMaxHours = 48;
struct Hour {
  time_t time = 0;
  int code = -1;
  float temperature = 0;
  int rainProbability = -1;
  int isDay = -1;
};
struct Weather {
  bool available = false;
  Hour current;
  bool isDay = true;
  Hour hours[kMaxHours];
  size_t count = 0;
};
struct Sensor {
  bool available = false;
  float temperature = 0;
  float humidity = 0;
  uint32_t ageMs = 0;
};
struct Snapshot {
  Weather weather;
  Sensor sensor;
  bool weatherError = false;
  bool sensorError = false;
  time_t weatherUpdated = 0;
  time_t sensorUpdated = 0;
  uint32_t weatherReceivedMs = 0;
  uint32_t sensorReceivedMs = 0;
};

// On malformed responses, leave the last successful result intact.
bool parseWeather(const char* json, size_t size, Weather& result);
bool parseSensor(const char* json, size_t size, Sensor& result);
const Hour* nextHour(const Weather& weather, time_t now);
// Current hour through the next 23 hours, including the following calendar day.
size_t upcomingHours(const Weather& weather, time_t now, size_t* indices, size_t capacity);
}  // namespace environment
