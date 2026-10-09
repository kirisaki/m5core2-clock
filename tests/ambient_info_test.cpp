#include "ambient_info.h"

#include <cassert>
#include <cmath>

int main() {
  using namespace ambient;
  environment::Weather weather;
  const time_t hour = 1767225600;
  const time_t now = hour + 1800;
  assert(rain(weather, now).state == RainState::Unknown);
  weather.available = true;
  weather.current.code = 0;
  weather.count = 25;
  for (size_t i = 0; i < weather.count; ++i) {
    weather.hours[i].time = hour + i * 3600;
    weather.hours[i].code = 0;
  }
  assert(rain(weather, now).state == RainState::None);
  weather.hours[2].code = 61;
  weather.hours[2].rainProbability = 70;
  auto forecast = rain(weather, now);
  assert(forecast.state == RainState::Starting && forecast.change == hour + 7200);
  assert(forecast.probability == 70);
  weather.hours[1].code = -1;
  assert(rain(weather, now).state == RainState::Unknown);
  weather.hours[1].code = 0;
  weather.current.code = 61;
  forecast = rain(weather, now);
  assert(forecast.state == RainState::Ongoing && forecast.change == hour + 3600);
  for (auto& item : weather.hours) item.code = 61;
  forecast = rain(weather, now);
  assert(forecast.complete && forecast.change == 0);
  weather.count = 8;
  assert(!rain(weather, now).complete);
  weather.current.code = 0;
  weather.hours[1].time += 3600;
  assert(rain(weather, now).state == RainState::Unknown);
  assert(rain(weather, 0).state == RainState::Unknown);

  weather.dayCount = 2;
  weather.days[0].sunrise = hour + 6 * 3600;
  weather.days[0].sunset = hour + 18 * 3600;
  weather.days[1].sunrise = hour + 30 * 3600;
  weather.days[1].sunset = hour + 42 * 3600;
  assert(nextSun(weather, now).rising);
  assert(nextSun(weather, hour + 6 * 3600).time == hour + 18 * 3600);
  assert(!nextSun(weather, hour + 6 * 3600).rising);
  assert(nextSun(weather, hour + 23 * 3600).time == hour + 30 * 3600);
  assert(nextSun(weather, hour + 43 * 3600).time == 0);
  weather.days[1].sunrise = 0;
  assert(nextSun(weather, hour + 23 * 3600).time == hour + 42 * 3600);
  assert(nextSun(weather, 0).time == 0);

  assert(std::isnan(moonAge(0)));
  // NASA/GSFC: new moon 2025-01-29 12:36 UTC. Mean-cycle estimate within one day.
  const double age = moonAge(1738154160);
  assert(age < 1 || age > kLunarMonth - 1);
  for (int day = 0; day < 365; ++day) {
    const double value = moonAge(hour + day * 86400);
    assert(value >= 0 && value < kLunarMonth);
  }

  environment::Snapshot data;
  assert(mood(data, 1000, false) == Mood::NoData);
  data.sensor.available = true;
  data.sensorReceivedMs = 1000;
  data.sensor.temperature = 25;
  data.sensor.humidity = 50;
  assert(mood(data, 1000, false) == Mood::Happy);
  assert(mood(data, 1000, true) == Mood::Sleepy);
  data.sensor.temperature = 17;
  assert(mood(data, 1000, false) == Mood::Cold);
  data.sensor.temperature = 29;
  assert(mood(data, 1000, false) == Mood::Hot);
  data.sensor.temperature = 25;
  data.sensor.humidity = 80;
  assert(mood(data, 1000, false) == Mood::Humid);
  data.sensor.humidity = 30;
  assert(mood(data, 1000, false) == Mood::Dry);
  assert(mood(data, 121000, false) == Mood::NoData);
  data.sensorError = true;
  assert(mood(data, 1000, false) == Mood::NoData);
}
