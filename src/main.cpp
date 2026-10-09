#include <Arduino.h>
#include <M5Unified.h>

#include "app_config.h"
#include "clock_service.h"
#include "network.h"
#include "oled.h"
#include "ui.h"

void setup() {
  auto cfg = M5.config();
  cfg.serial_baudrate = 115200;
  cfg.internal_imu = false;
  cfg.internal_mic = false;
  cfg.internal_spk = false;
  M5.begin(cfg);
  M5.Display.setRotation(1);
  M5.Display.setBrightness(config::kBrightness);

  clock_service::begin();
  ui::begin();
  ui::update();
  oled::begin();
  network::begin();
}

void loop() {
  M5.update();
  clock_service::update();
  // One queue consumer shares the same snapshot with both displays.
  static environment::Snapshot snapshot;
  if (network::receive(snapshot)) {
    ui::setData(snapshot);
    oled::setData(snapshot);
  }
  ui::update();
  oled::update();
  delay(10);
}
