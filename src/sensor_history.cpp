#include "sensor_history.h"

#include <cmath>

#include "data_freshness.h"

void environment::SensorHistory::expire(uint32_t nowMs) {
  while (count_ && nowMs - at(0).receivedMs > kWindowMs) {
    start_ = (start_ + 1) % kCapacity;
    --count_;
  }
}

void environment::SensorHistory::observe(const Snapshot& snapshot, uint32_t nowMs) {
  expire(nowMs);
  const auto& sensor = snapshot.sensor;
  if (!sensor.available || sensorStale(snapshot, nowMs) ||
      !std::isfinite(sensor.temperature) || !std::isfinite(sensor.humidity)) {
    interrupted_ = true;
    return;
  }
  // Weather updates carry the same sensor reading; do not record it again.
  if (seen_ && lastReceivedMs_ == snapshot.sensorReceivedMs) return;
  seen_ = true;
  lastReceivedMs_ = snapshot.sensorReceivedMs;
  const uint32_t elapsed = count_ ? snapshot.sensorReceivedMs - at(count_ - 1).receivedMs : 0;
  if (count_ && elapsed < kIntervalMs) return;
  const bool connected = count_ && !interrupted_ && elapsed <= 90 * 1000;
  if (count_ == kCapacity) {
    start_ = (start_ + 1) % kCapacity;
    --count_;
  }
  auto& sample = samples_[(start_ + count_) % kCapacity];
  sample.receivedMs = snapshot.sensorReceivedMs;
  sample.temperature = sensor.temperature;
  sample.humidity = sensor.humidity;
  sample.connected = connected;
  ++count_;
  interrupted_ = false;
}
