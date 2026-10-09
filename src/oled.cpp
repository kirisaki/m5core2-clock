#include "oled.h"

#include <M5Unified.h>
#include <U8g2lib.h>
#include <cstring>

#include "clock_service.h"
#include "oled_renderer.h"

namespace {
// 2.42OLED-IIC VER:1.1 does not return ACK unless D2 is bridged.
// Software I2C supplies the ninth clock but ignores ACK.
// The installed cable connects white GPIO33 to SDA and yellow GPIO32 to SCL.
// This assignment was verified on the OLED with test B. RTC/touch use another bus.
#ifdef CLOCK_OLED_PIN_TEST
constexpr uint8_t kSda = 32;
constexpr uint8_t kScl = 33;
#else
constexpr uint8_t kSda = 33;
constexpr uint8_t kScl = 32;
#endif
// NONAME0 starts at column 0; NONAME2 shifts output two pixels to the right.
U8G2_SSD1309_128X64_NONAME0_F_SW_I2C display(U8G2_R0, kScl, kSda, U8X8_PIN_NONE);
bool ready = false;
time_t lastSecond = -1;
#ifdef CLOCK_OLED_PIN_TEST
bool swapped = false;
uint32_t modeStarted = 0;

const char* testLabel() {
  return swapped ? "B SDA33 SCL32" : "A SDA32 SCL33";
}
#endif

}  // namespace

void oled::begin(const environment::Snapshot& data) {
  Serial.printf("OLED: external power output %s\n", M5.Power.getExtOutput() ? "enabled" : "disabled");
  uint8_t address = 0x3C;  // Board label 0x78 is the 8-bit write address.
  bool acknowledged = false;
  if (M5.Ex_I2C.begin(M5.Ex_I2C.getPort(), kSda, kScl)) {
    for (uint8_t candidate : {0x3C, 0x3D}) {
      if (M5.Ex_I2C.scanID(candidate)) {
        address = candidate;
        acknowledged = true;
        break;
      }
    }
  }
  // Release the hardware peripheral before software I2C takes over these pins.
  M5.Ex_I2C.release();
  Serial.printf("OLED: SSD1309 128x64 SDA=%u SCL=%u address=0x%02X (%s)\n",
                kSda, kScl, address, acknowledged ? "ACK received" : "no ACK; sending anyway");
  display.setI2CAddress(address << 1);
  display.setBusClock(100000);
  if (!display.begin()) {
    Serial.println("OLED: software I2C setup failed");
    return;
  }
  display.setContrast(128);
  Serial.printf("OLED: size=%ux%u x_offset=%u\n",
                display.getDisplayWidth(), display.getDisplayHeight(),
                display.getU8x8()->x_offset);
  ready = true;
#ifdef CLOCK_OLED_PIN_TEST
  modeStarted = millis();
  Serial.printf("OLED pin test: %s\n", testLabel());
#endif
  update(data);
  Serial.println("OLED: frame sent without ACK checking; check screen visually");
}

void oled::update(const environment::Snapshot& data) {
  if (!ready) return;
#ifdef CLOCK_OLED_PIN_TEST
  if (millis() - modeStarted >= 10000) {
    swapped = !swapped;
    // Both signal pins remain open-drain, including when exchanging roles.
    pinMode(32, INPUT_PULLUP);
    pinMode(33, INPUT_PULLUP);
    u8x8_SetPin_SW_I2C(display.getU8x8(), swapped ? 32 : 33,
                      swapped ? 33 : 32, U8X8_PIN_NONE);
    display.begin();
    display.setContrast(128);
    modeStarted = millis();
    lastSecond = -1;
    Serial.printf("OLED pin test: %s\n", testLabel());
  }
#endif
  const time_t second = time(nullptr);
  if (second == lastSecond) return;
  lastSecond = second;
  oled_renderer::FrameTime frame;
  frame.utc = second;
  frame.valid = clock_service::localTime(frame.local);
  frame.uptimeMs = millis();
  display.clearBuffer();
#ifdef CLOCK_OLED_PIN_TEST
  oled_renderer::pinTest(display, frame, testLabel());
#else
  oled_renderer::dashboard(display, data, frame);
#endif
#ifndef CLOCK_OLED_PIN_TEST
  static uint8_t previous[1024];
  static bool sent = false;
  if (sent && std::memcmp(previous, display.getBufferPtr(), sizeof(previous)) == 0) return;
  std::memcpy(previous, display.getBufferPtr(), sizeof(previous));
  sent = true;
#endif
  display.sendBuffer();
#ifdef CLOCK_OLED_PIN_TEST
  // Temporary diagnostic status replaces only the LCD's network status line.
  M5.Display.fillRect(0, 219, 320, 10, TFT_BLACK);
  M5.Display.setFont(&fonts::Font0);
  M5.Display.setTextColor(TFT_YELLOW, TFT_BLACK);
  M5.Display.drawString(testLabel(), 12, 219);
#endif
}
