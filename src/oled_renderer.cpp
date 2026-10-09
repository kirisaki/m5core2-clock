#include "oled_renderer.h"

#include <U8g2lib.h>
#include <cmath>
#include <cstdio>

#include "ambient_info.h"
#include "data_freshness.h"

namespace {
void centered(U8G2& display, const char* text, int baseline, int centerX = 64) {
  display.drawStr(centerX - display.getStrWidth(text) / 2, baseline, text);
}

void creature(U8G2& display, ambient::Mood mood, uint32_t tick) {
  const bool blink = tick % 9 == 0;
  const int bob = (tick / 3) % 2;
  const char* caption = "COMFY";
  int eyeY = 33 + bob;
  display.setFont(u8g2_font_5x7_tf);
  centered(display, "ROOMIE", 8, 22);
  if (mood == ambient::Mood::Humid) {
    caption = "MELTING";
    display.drawFilledEllipse(22, 45, 19, 7);
    display.drawRBox(6, 32 + bob, 31, 16, 7);
    eyeY = 39 + bob;
  } else if (mood == ambient::Mood::Cold) {
    caption = "BRRR";
    display.drawDisc(22, 37 + bob, 13);
    display.drawLine(2, 30, 4, 33);
    display.drawLine(4, 33, 2, 36);
    display.drawLine(40, 30, 42, 33);
    display.drawLine(42, 33, 40, 36);
    eyeY = 37 + bob;
  } else {
    display.drawRBox(7, 23 + bob, 29, 27, 10);
    display.drawTriangle(10, 27 + bob, 12, 16 + bob, 18, 25 + bob);
    display.drawTriangle(25, 25 + bob, 31, 17 + bob, 33, 28 + bob);
    display.drawBox(10, 47 + bob, 7, 4);
    display.drawBox(27, 47 + bob, 7, 4);
  }
  display.setDrawColor(0);
  if (blink || mood == ambient::Mood::Sleepy || mood == ambient::Mood::Cold) {
    display.drawHLine(13, eyeY, 5);
    display.drawHLine(26, eyeY, 5);
  } else {
    display.drawBox(14, eyeY - 2, 3, 4);
    display.drawBox(27, eyeY - 2, 3, 4);
  }
  if (mood == ambient::Mood::Hot || mood == ambient::Mood::Dry)
    display.drawCircle(22, eyeY + 7, 2);
  else {
    display.drawLine(19, eyeY + 6, 22, eyeY + 8);
    display.drawLine(22, eyeY + 8, 25, eyeY + 6);
  }
  display.setDrawColor(1);
  if (mood == ambient::Mood::Hot) {
    caption = "HOT!";
    display.drawTriangle(39, 21 + bob, 37, 26 + bob, 41, 26 + bob);
    display.drawDisc(39, 27 + bob, 2);
  } else if (mood == ambient::Mood::Dry) {
    caption = "DRY";
    display.drawLine(2, 23, 5, 25);
    display.drawLine(5, 25, 2, 28);
  } else if (mood == ambient::Mood::Sleepy) {
    caption = "ZZZ";
    display.drawStr(35, 20 - bob, "z");
  } else if (mood == ambient::Mood::NoData) {
    caption = "NO DATA";
    display.drawStr(35, 20, "?");
  }
  centered(display, caption, 62, 22);
}

void rainIcon(U8G2& display) {
  display.drawDisc(55, 10, 7, U8G2_DRAW_UPPER_LEFT | U8G2_DRAW_UPPER_RIGHT);
  display.drawVLine(55, 10, 7);
  display.drawLine(55, 16, 52, 18);
}

void sunIcon(U8G2& display, bool rising) {
  display.drawCircle(55, 32, 5, U8G2_DRAW_UPPER_LEFT | U8G2_DRAW_UPPER_RIGHT);
  display.drawHLine(48, 33, 15);
  display.drawVLine(55, 22, 3);
  display.drawLine(48, 25, 50, 27);
  display.drawLine(60, 27, 62, 25);
  const int y = rising ? 36 : 39;
  display.drawLine(52, rising ? y + 3 : y - 3, 55, y);
  display.drawLine(55, y, 58, rising ? y + 3 : y - 3);
}

void moonIcon(U8G2& display, double age) {
  constexpr int radius = 7;
  if (!std::isfinite(age)) {
    display.drawCircle(55, 53, radius);
    display.drawStr(53, 56, "?");
    return;
  }
  const double phase = age / ambient::kLunarMonth;
  const double terminator = std::cos(phase * 6.283185307179586);
  // Waxing is lit on the right; waning on the left (Northern Hemisphere).
  for (int y = -radius; y <= radius; ++y) {
    const double edge = std::sqrt(static_cast<double>(radius * radius - y * y));
    for (int x = -radius; x <= radius; ++x) {
      if (x * x + y * y > radius * radius) continue;
      if (phase < 0.5 ? x >= terminator * edge : x <= -terminator * edge)
        display.drawPixel(55 + x, 53 + y);
    }
  }
  // Outline only the nearly new moon; a full outline hides a thin crescent.
  if (phase < 0.02 || phase > 0.98) display.drawCircle(55, 53, radius);
}

void countdown(time_t event, time_t now, char* text, size_t size) {
  const unsigned minutes = static_cast<unsigned>((event - now + 59) / 60);
  if (minutes < 60) snprintf(text, size, "%um", minutes);
  else snprintf(text, size, "%uh %02um", minutes / 60, minutes % 60);
}

}  // namespace

void oled_renderer::dashboard(U8G2& display, const environment::Snapshot& data, const FrameTime& frame) {
  const time_t now = frame.utc;
  const bool valid = frame.valid;
  const tm& local = frame.local;
  const bool night = valid && (local.tm_hour >= 23 || local.tm_hour < 6);
  creature(display, ambient::mood(data, frame.uptimeMs, night), frame.uptimeMs / 1000);
  display.drawVLine(44, 0, 64);
  display.setFont(u8g2_font_5x7_tf);
  const bool stale = environment::weatherStale(data, frame.uptimeMs);
  const bool weatherReady = valid && data.weather.available && !stale;
  rainIcon(display);
  const auto rain = weatherReady ? ambient::rain(data.weather, now) : ambient::Rain{};
  const char* heading = "RAIN";
  char text[16] = "--";
  if (data.weather.available && stale) snprintf(text, sizeof(text), "OLD");
  if (rain.state == ambient::RainState::None) {
    heading = "NO RAIN";
    snprintf(text, sizeof(text), "NEXT 24h");
  } else if (rain.state == ambient::RainState::Starting || rain.state == ambient::RainState::Ongoing) {
    heading = rain.state == ambient::RainState::Starting ? "RAIN IN" : "STOPS IN";
    if (rain.change) {
      const unsigned hours = static_cast<unsigned>((rain.change - now + 3599) / 3600);
      if (rain.state == ambient::RainState::Starting && rain.probability >= 0)
        snprintf(text, sizeof(text), "~%uh %d%%", hours, rain.probability);
      else snprintf(text, sizeof(text), "~%uh", hours);
    } else if (rain.complete) snprintf(text, sizeof(text), ">24h");
    else snprintf(text, sizeof(text), "--");
  }
  display.drawStr(67, 8, heading);
  display.drawStr(67, 18, text);

  const auto sun = weatherReady ? ambient::nextSun(data.weather, now) : ambient::Sun{};
  sunIcon(display, sun.rising);
  display.drawStr(67, 29, sun.time ? (sun.rising ? "SUNRISE IN" : "SUNSET IN") : "SUN");
  snprintf(text, sizeof(text), "%s", data.weather.available && stale ? "OLD" : "--");
  if (sun.time) countdown(sun.time, now, text, sizeof(text));
  display.drawStr(67, 39, text);

  const double age = valid ? ambient::moonAge(now) : NAN;
  moonIcon(display, age);
  display.drawStr(67, 50, "MOON AGE");
  snprintf(text, sizeof(text), "--");
  if (std::isfinite(age)) snprintf(text, sizeof(text), "~%.1fd", age);
  display.drawStr(67, 61, text);
}

void oled_renderer::pinTest(U8G2& display, const FrameTime& frame, const char* label) {
  char date[24] = "Waiting for NTP";
  char clock[16] = "--:--:--";
  if (frame.valid) {
    strftime(date, sizeof(date), "%Y-%m-%d %a", &frame.local);
    strftime(clock, sizeof(clock), "%H:%M:%S", &frame.local);
  }
  display.drawFrame(0, 0, 128, 64);
  display.setFont(u8g2_font_6x10_tf);
  centered(display, date, 14);
  display.setFont(u8g2_font_helvB18_tf);
  centered(display, clock, 40);
  display.setFont(u8g2_font_6x10_tf);
  centered(display, label, 56);
}
