#include <Arduino.h>
#include <M5Unified.h>

#include "app_config.h"
#include "app_state.h"
#include "clock_service.h"
#include "network.h"
#include "oled.h"
#include "ui.h"

namespace {
app::State state;
bool dimmed = false;
constexpr uint8_t kButtonVibration = 200;
constexpr uint32_t kButtonVibrationMs = 160;
}  // namespace

void setup() {
  auto cfg = M5.config();
  cfg.serial_baudrate = 115200;
  cfg.internal_imu = false;
  cfg.internal_mic = false;
  cfg.internal_spk = false;
  M5.begin(cfg);
  M5.Display.setRotation(1);
  M5.Display.setBrightness(config::kBrightness);
  M5.Power.setVibration(0);

  clock_service::begin();
  ui::begin();
  ui::update(state);
  oled::begin(state.data());
  network::begin();
}

void loop() {
  M5.update();
  if (M5.BtnB.wasPressed()) {
    dimmed = !dimmed;
    oled::setDimmed(dimmed);
    M5.Display.setBrightness(dimmed ? config::kDimBrightness : config::kBrightness);
    // Complete a short pulse before display transfers can extend its duration.
    // All PMIC/I2C operations stay on the main task.
    M5.Power.setVibration(kButtonVibration);
    delay(kButtonVibrationMs);
    M5.Power.setVibration(0);
  }
  clock_service::update();
  // One queue consumer shares the same snapshot with both displays.
  static environment::Snapshot snapshot;
  if (network::receive(snapshot)) {
    state.update(snapshot, millis());
  }
  state.tick(millis());
  ui::update(state);
  oled::update(state.data());
  delay(10);
}
