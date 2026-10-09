#include "data_freshness.h"

#include <cassert>
#include <cstdint>

int main() {
  environment::Snapshot data;
  data.sensorReceivedMs = 1000;
  data.weatherReceivedMs = 1000;
  data.sensor.ageMs = 400;
  assert(!environment::sensorStale(data, 120599));
  assert(environment::sensorStale(data, 120600));
  assert(!environment::weatherStale(data, 1800999));
  assert(environment::weatherStale(data, 1801000));
  data.sensorError = data.weatherError = true;
  assert(environment::sensorStale(data, 1000));
  assert(environment::weatherStale(data, 1000));
  data.sensorError = data.weatherError = false;
  data.sensor.ageMs = UINT32_MAX;
  assert(environment::sensorStale(data, 1001));  // API age must not overflow.
  data.sensor.ageMs = 0;
  data.sensorReceivedMs = data.weatherReceivedMs = UINT32_MAX - 999;
  assert(!environment::sensorStale(data, 0));
  assert(!environment::weatherStale(data, 0));
  assert(environment::sensorStale(data, 119000));
  assert(environment::weatherStale(data, 1799000));
}
