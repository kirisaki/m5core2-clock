#pragma once

#include "environment_data.h"

namespace environment {
struct SensorSample {
  uint32_t receivedMs = 0;
  float temperature = 0;
  float humidity = 0;
  bool connected = false;  // A continuous series with the preceding sample.
};

class SensorHistory {
 public:
  static constexpr uint32_t kWindowMs = 12 * 60 * 60 * 1000;
  static constexpr uint32_t kIntervalMs = 60 * 1000;
  static constexpr size_t kCapacity = kWindowMs / kIntervalMs + 1;

  void observe(const Snapshot& snapshot, uint32_t nowMs);
  // Call regularly, including while offline, to evict expired readings.
  void expire(uint32_t nowMs);
  size_t size() const { return count_; }
  const SensorSample& at(size_t index) const { return samples_[(start_ + index) % kCapacity]; }

 private:
  SensorSample samples_[kCapacity];
  size_t start_ = 0;
  size_t count_ = 0;
  bool interrupted_ = true;
  bool seen_ = false;
  uint32_t lastReceivedMs_ = 0;
};
}  // namespace environment
