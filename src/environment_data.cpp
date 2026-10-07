#include "environment_data.h"

#include <ArduinoJson.h>
#include <cmath>

namespace {
bool number(JsonVariantConst value, float minimum, float maximum) {
  return value.is<float>() && std::isfinite(value.as<float>()) &&
         value.as<float>() >= minimum && value.as<float>() <= maximum;
}
bool timestamp(JsonVariantConst value) {
  return value.is<int64_t>() && value.as<int64_t>() >= 1704067200 &&
         value.as<int64_t>() <= 2147483647;
}
bool weatherCode(JsonVariantConst value) {
  return value.is<int>() && value.as<int>() >= 0 && value.as<int>() <= 99;
}
}  // namespace

bool environment::parseSensor(const char* json, size_t size, Sensor& result) {
  JsonDocument doc;
  if (deserializeJson(doc, json, size, DeserializationOption::NestingLimit(8))) return false;
  if (!number(doc["temperature_c"], -100, 100) ||
      !number(doc["humidity_percent"], 0, 100) || !doc["age_ms"].is<uint32_t>()) return false;
  Sensor next;
  next.available = true;
  next.temperature = doc["temperature_c"];
  next.humidity = doc["humidity_percent"];
  next.ageMs = doc["age_ms"];
  result = next;
  return true;
}

bool environment::parseWeather(const char* json, size_t size, Weather& result) {
  JsonDocument doc;
  if (deserializeJson(doc, json, size, DeserializationOption::NestingLimit(8))) return false;
  const JsonObjectConst current = doc["current"];
  const JsonArrayConst times = doc["hourly"]["time"];
  const JsonArrayConst codes = doc["hourly"]["weather_code"];
  const JsonArrayConst temperatures = doc["hourly"]["temperature_2m"];
  const JsonArrayConst rain = doc["hourly"]["precipitation_probability"];
  const JsonArrayConst daylight = doc["hourly"]["is_day"];
  if (!timestamp(current["time"]) || !weatherCode(current["weather_code"]) ||
      !number(current["temperature_2m"], -100, 100) ||
      !current["is_day"].is<int>() ||
      (current["is_day"].as<int>() != 0 && current["is_day"].as<int>() != 1) ||
      times.size() == 0 || times.size() > kMaxHours ||
      codes.size() != times.size() || temperatures.size() != times.size() ||
      rain.size() != times.size() ||
      (!daylight.isNull() && daylight.size() != times.size())) return false;

  Weather next;
  next.current.time = current["time"].as<time_t>();
  next.current.code = current["weather_code"];
  next.current.temperature = current["temperature_2m"];
  next.isDay = current["is_day"].as<int>() == 1;
  next.count = times.size();
  for (size_t i = 0; i < next.count; ++i) {
    if (!timestamp(times[i]) || (i && times[i].as<time_t>() != next.hours[i - 1].time + 3600))
      return false;
    auto& hour = next.hours[i];
    hour.time = times[i].as<time_t>();
    // Null forecast fields are missing data, never clear skies / zero degrees.
    if (!codes[i].isNull() && !weatherCode(codes[i])) return false;
    if (!temperatures[i].isNull() && !number(temperatures[i], -100, 100)) return false;
    if (!rain[i].isNull() && !number(rain[i], 0, 100)) return false;
    if (!daylight[i].isNull() && (!daylight[i].is<int>() ||
        (daylight[i].as<int>() != 0 && daylight[i].as<int>() != 1))) return false;
    hour.code = codes[i].isNull() ? -1 : codes[i].as<int>();
    hour.temperature = temperatures[i].isNull() ? NAN : temperatures[i].as<float>();
    hour.rainProbability = rain[i].isNull() ? -1 : static_cast<int>(rain[i].as<float>());
    hour.isDay = daylight[i].isNull() ? -1 : daylight[i].as<int>();
  }
  next.available = true;
  result = next;
  return true;
}

const environment::Hour* environment::nextHour(const Weather& weather, time_t now) {
  for (size_t i = 0; i < weather.count; ++i) {
    if (weather.hours[i].time > now) return &weather.hours[i];
  }
  return nullptr;
}

size_t environment::upcomingHours(const Weather& weather, time_t now, size_t* indices, size_t capacity) {
  const time_t first = now - now % 3600;
  size_t count = 0;
  for (size_t i = 0; i < weather.count && count < capacity; ++i) {
    if (weather.hours[i].time >= first && weather.hours[i].time - first < 24 * 3600)
      indices[count++] = i;
  }
  return count;
}
