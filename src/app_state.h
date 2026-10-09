#pragma once

#include "sensor_history.h"

namespace app {
// Owned by the main task; displays only receive const access.
class State {
 public:
  void update(const environment::Snapshot& snapshot, uint32_t nowMs) {
    data_ = snapshot;
    history_.observe(data_, nowMs);
    ++revision_;
  }
  void tick(uint32_t nowMs) { history_.expire(nowMs); }

  const environment::Snapshot& data() const { return data_; }
  const environment::SensorHistory& history() const { return history_; }
  uint32_t revision() const { return revision_; }

 private:
  environment::Snapshot data_;
  environment::SensorHistory history_;
  uint32_t revision_ = 0;
};
}  // namespace app
