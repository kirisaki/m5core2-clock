#include "ambient_info.h"

#include <cmath>

#include "data_freshness.h"

namespace {
int rainCode(int code) {
  switch (code) {
    case 51: case 53: case 55: case 56: case 57:
    case 61: case 63: case 65: case 66: case 67:
    case 80: case 81: case 82: case 95: case 96: case 99: return 1;
    case 0: case 1: case 2: case 3: case 45: case 48:
    case 71: case 73: case 75: case 77: case 85: case 86: return 0;
    default: return -1;
  }
}
}

ambient::Rain ambient::rain(const environment::Weather& weather, time_t now) {
  Rain result;
  if (!weather.available || now < 1704067200) return result;
  const int current = rainCode(weather.current.code);
  if (current < 0) return result;
  if (current) result.state = RainState::Ongoing;
  time_t expected = now - now % 3600 + 3600;
  size_t checked = 0;
  for (size_t i = 0; i < weather.count && checked < 24; ++i) {
    const auto& hour = weather.hours[i];
    if (hour.time < expected) continue;
    // Missing data must not imply no rain or a known time for rain to stop.
    if (hour.time != expected || rainCode(hour.code) < 0) return result;
    if (rainCode(hour.code) != current) {
      result.state = current ? RainState::Ongoing : RainState::Starting;
      result.change = hour.time;
      result.probability = hour.rainProbability;
      return result;
    }
    expected += 3600;
    ++checked;
  }
  result.complete = checked == 24;
  if (!current && result.complete) result.state = RainState::None;
  return result;
}

ambient::Sun ambient::nextSun(const environment::Weather& weather, time_t now) {
  Sun result;
  if (!weather.available || now < 1704067200) return result;
  for (size_t i = 0; i < weather.dayCount; ++i) {
    const auto& day = weather.days[i];
    if (day.sunrise > now && (!result.time || day.sunrise < result.time)) {
      result.time = day.sunrise;
      result.rising = true;
    }
    if (day.sunset > now && (!result.time || day.sunset < result.time)) {
      result.time = day.sunset;
      result.rising = false;
    }
  }
  return result;
}

double ambient::moonAge(time_t now) {
  if (now < 1704067200) return NAN;
  // New moon 2001-01-24 13:07 UT and mean synodic month (NASA/GSFC):
  // https://eclipse.gsfc.nasa.gov/phase/phase2001gmt.html
  constexpr double reference = 980341620.0;
  double age = std::fmod((static_cast<double>(now) - reference) / 86400.0, kLunarMonth);
  if (age < 0) age += kLunarMonth;
  return age;
}

ambient::Mood ambient::mood(const environment::Snapshot& data, uint32_t nowMs, bool night) {
  if (!data.sensor.available || environment::sensorStale(data, nowMs))
    return Mood::NoData;
  if (data.sensor.temperature < 18) return Mood::Cold;
  if (data.sensor.temperature >= 28) return Mood::Hot;
  if (data.sensor.humidity >= 70) return Mood::Humid;
  if (data.sensor.humidity < 35) return Mood::Dry;
  return night ? Mood::Sleepy : Mood::Happy;
}
