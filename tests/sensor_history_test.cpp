#include "sensor_history.h"

#include <cassert>
#include <cmath>
#include <cstdint>

using environment::SensorHistory;
using environment::Snapshot;

Snapshot reading(uint32_t receivedMs, float temperature = 25, float humidity = 50) {
  Snapshot snapshot;
  snapshot.sensor.available = true;
  snapshot.sensor.temperature = temperature;
  snapshot.sensor.humidity = humidity;
  snapshot.sensorReceivedMs = receivedMs;
  return snapshot;
}

int main() {
  SensorHistory history;
  history.observe(Snapshot{}, 0);
  assert(history.size() == 0);
  history.observe(reading(1000), 1000);
  assert(history.size() == 1 && !history.at(0).connected);
  history.observe(reading(31000, 26), 31000);
  assert(history.size() == 1);  // At most one sample per minute.
  auto next = reading(61000, 27, 60);
  next.sensorUpdated = 1800000000;  // Wall clock changes do not affect history.
  history.observe(next, 61000);
  assert(history.size() == 2 && history.at(1).temperature == 27);
  assert(history.at(1).humidity == 60 && history.at(1).connected);
  next.weatherUpdated = 1800000010;
  history.observe(next, 71000);
  assert(history.size() == 2);  // Weather snapshots must not duplicate sensor data.

  next.sensorError = true;
  history.observe(next, 91000);
  history.observe(reading(121000), 121000);
  assert(history.size() == 3 && !history.at(2).connected);
  history.observe(reading(241000), 241000);
  assert(history.size() == 4 && !history.at(3).connected);  // Offline gap.
  next = reading(301000);
  next.sensor.ageMs = 120000;
  history.observe(next, 301000);
  assert(history.size() == 4);
  history.observe(reading(361000, NAN), 361000);
  history.observe(reading(361000, 25, INFINITY), 361000);
  assert(history.size() == 4);
  history.observe(reading(421000), 541000);  // Delayed snapshot is also stale.
  assert(history.size() == 4);
  history.expire(241001 + SensorHistory::kWindowMs);
  assert(history.size() == 0);

  // More than one day's input stays bounded and keeps samples in chronological order.
  SensorHistory rolling;
  for (uint32_t minute = 0; minute <= 1500; ++minute) {
    const uint32_t now = minute * 60000;
    rolling.observe(reading(now), now);
  }
  assert(rolling.size() == 721);
  assert(rolling.at(0).receivedMs == 780 * 60000);
  assert(rolling.at(720).receivedMs == 1500 * 60000);
  for (size_t i = 1; i < rolling.size(); ++i) {
    assert(rolling.at(i).receivedMs - rolling.at(i - 1).receivedMs == 60000);
    assert(rolling.at(i).connected);
  }

  // millis() wraps roughly every 49 days; recording and eviction must still work.
  SensorHistory wrapped;
  const uint32_t start = UINT32_MAX - 30000;
  wrapped.observe(reading(start), start);
  wrapped.observe(reading(start + 60000u), start + 60000u);
  assert(wrapped.size() == 2 && wrapped.at(1).connected);
  wrapped.expire(start + SensorHistory::kWindowMs + 1u);
  assert(wrapped.size() == 1);
  wrapped.expire(start + SensorHistory::kWindowMs + 60001u);
  assert(wrapped.size() == 0);
}
