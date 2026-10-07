#include "environment_data.h"
#include <ArduinoJson.h>
#include <cassert>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>

using namespace environment;

bool parse(JsonDocument& doc, Weather& weather) {
  std::string body;
  serializeJson(doc, body);
  return parseWeather(body.c_str(), body.size(), weather);
}

int main(int argc, char** argv) {
  setenv("TZ", "JST-9", 1);
  tzset();
  tm utc{};
  utc.tm_year = 126;
  utc.tm_mon = 0;
  utc.tm_mday = 1;
  utc.tm_hour = 14;  // Jan 1, 23:00 JST.
  const time_t start = timegm(&utc);

  JsonDocument doc;
  doc["current"]["time"] = start + 900;
  doc["current"]["weather_code"] = 2;
  doc["current"]["temperature_2m"] = 12.5;
  doc["current"]["is_day"] = 0;
  for (size_t i = 0; i < kMaxHours; ++i) {
    doc["hourly"]["time"][i] = start + i * 3600;
    doc["hourly"]["weather_code"][i] = 61;
    doc["hourly"]["temperature_2m"][i] = 15.5;
    doc["hourly"]["precipitation_probability"][i] = 30;
    doc["hourly"]["is_day"][i] = i % 2;
  }
  Weather weather;
  assert(parse(doc, weather));
  assert(weather.available && weather.count == 48 && !weather.isDay);
  assert(weather.current.code == 2 && weather.current.temperature == 12.5);
  assert(weather.hours[0].isDay == 0 && weather.hours[1].isDay == 1);
  assert(nextHour(weather, start + 3599)->time == start + 3600);
  assert(nextHour(weather, start + 3600)->time == start + 7200);
  assert(nextHour(weather, start + 47 * 3600) == nullptr);
  size_t indices[kMaxHours];
  // 23:59 JST still starts at 23:00, then crosses midnight on the same page.
  assert(upcomingHours(weather, start + 3599, indices, kMaxHours) == 24);
  assert(indices[0] == 0 && indices[11] == 11 && indices[12] == 12 && indices[23] == 23);
  assert(weather.hours[indices[12]].time == weather.hours[indices[11]].time + 3600);
  assert(upcomingHours(weather, start + 3600, indices, kMaxHours) == 24 && indices[0] == 1);
  assert(upcomingHours(weather, start + 3600, indices, 12) == 12);
  assert(upcomingHours(weather, start + 47 * 3600, indices, kMaxHours) == 1);
  assert(upcomingHours(weather, start + 48 * 3600, indices, kMaxHours) == 0);
  doc["hourly"]["is_day"][0] = 2;
  assert(!parse(doc, weather));
  doc["hourly"]["is_day"][0] = nullptr;
  assert(parse(doc, weather) && weather.hours[0].isDay == -1);

  doc["hourly"]["temperature_2m"][0] = nullptr;
  doc["hourly"]["weather_code"][0] = nullptr;
  doc["hourly"]["precipitation_probability"][0] = nullptr;
  assert(parse(doc, weather));
  assert(std::isnan(weather.hours[0].temperature));
  assert(weather.hours[0].code == -1 && weather.hours[0].rainProbability == -1);
  doc["hourly"]["time"][1] = start;  // Duplicate / out-of-order time.
  assert(!parse(doc, weather));
  assert(weather.count == 48 && weather.hours[1].time == start + 3600);
  doc["hourly"]["time"][1] = start + 3600;
  doc["hourly"]["precipitation_probability"][1] = 101;
  assert(!parse(doc, weather));
  doc["hourly"]["precipitation_probability"][1] = 30;
  doc["hourly"]["weather_code"].as<JsonArray>().remove(47);
  assert(!parse(doc, weather));
  assert(!parseWeather("{\"error\":true}", 14, weather));
  assert(!parseWeather("{", 1, weather));
  assert(weather.available && weather.current.code == 2);

  Sensor sensor;
  const std::string valid = R"({"temperature_c":27.22,"humidity_percent":51.77,"sensor":"SHT40","age_ms":431})";
  assert(parseSensor(valid.c_str(), valid.size(), sensor));
  assert(sensor.available && sensor.ageMs == 431);
  for (const std::string invalid : {
      R"({"temperature_c":null,"humidity_percent":50,"age_ms":0})",
      R"({"temperature_c":"22","humidity_percent":50,"age_ms":0})",
      R"({"temperature_c":22,"humidity_percent":101,"age_ms":0})",
      R"({"temperature_c":22,"humidity_percent":50})",
      R"({"temperature_c":22,"humidity_percent":50,"age_ms":-1})",
  }) {
    assert(!parseSensor(invalid.c_str(), invalid.size(), sensor));
    assert(sensor.available && sensor.ageMs == 431);
  }

  if (argc == 2) {
    std::ifstream input(argv[1]);
    assert(input.good());
    const std::string body{std::istreambuf_iterator<char>(input), {}};
    assert(parseWeather(body.c_str(), body.size(), weather));
    assert(weather.count == 48);
    std::cout << "Live Open-Meteo response: OK\n";
  }
  std::cout << "API parsing, missing data, cache retention, JST midnight: OK\n";
}
