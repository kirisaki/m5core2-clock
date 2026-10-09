#include "app_state.h"

#include <cassert>

int main() {
  app::State state;
  assert(state.revision() == 0 && state.history().size() == 0);
  environment::Snapshot input;
  input.sensor.available = true;
  input.sensor.temperature = 25;
  input.sensor.humidity = 50;
  input.sensorReceivedMs = 1000;
  state.update(input, 1000);
  assert(state.revision() == 1 && state.history().size() == 1);
  // State owns its values independently of the network queue's receive buffer.
  input.sensor.temperature = 30;
  assert(state.data().sensor.temperature == 25);
  input.sensorReceivedMs = 61000;
  state.update(input, 61000);
  assert(state.history().size() == 2 && state.history().at(1).temperature == 30);
  input.weather.available = true;
  state.update(input, 62000);
  assert(state.revision() == 3 && state.data().weather.available);
  assert(state.history().size() == 2);  // Weather updates don't duplicate history.
  input.sensorError = true;
  state.update(input, 91000);
  input.sensorError = false;
  input.sensorReceivedMs = 121000;
  state.update(input, 121000);
  assert(state.history().size() == 3 && !state.history().at(2).connected);
  // No display or network activity is required to expire old samples.
  state.tick(121001 + environment::SensorHistory::kWindowMs);
  assert(state.history().size() == 0);
  assert(state.data().sensor.temperature == 30 && state.revision() == 5);
}
