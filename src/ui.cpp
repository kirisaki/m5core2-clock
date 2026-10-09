#include "ui.h"

#include <M5Unified.h>
#include <cmath>
#include <ctime>
#include <cstdlib>

#include "data_freshness.h"
#include "clock_service.h"
#include "network.h"
#include "sensor_history.h"

namespace {
constexpr uint32_t kBackground = 0x101820;
constexpr uint32_t kCard = 0x203040;
constexpr uint32_t kText = 0xF0F4F8;
constexpr uint32_t kMuted = 0xA0B0C0;
constexpr uint32_t kAccent = 0x65D6C4;
M5Canvas canvas(&M5.Display);
bool ready = false;
enum class Screen { Home, Forecast, History };
Screen screen = Screen::Home;
bool dirty = true;
time_t lastSecond = -1;
network::Status lastStatus = network::Status::Unconfigured;
uint32_t lastRevision = 0;
size_t forecastIndices[24];
size_t forecastCount = 0;
size_t page = 0;
constexpr size_t kHoursPerPage = 12;
constexpr int kHomeRowTop = 136;
constexpr int kHomeRowHeight = 76;
constexpr int kHomeWeatherLeft = 112;
constexpr int kForecastGridTop = 46;
constexpr int kForecastRowHeight = 73;
constexpr int kForecastNavTop = 193;
constexpr int kForecastNavBottom = 219;
constexpr int kBackSwipeEdgeWidth = 24;
constexpr int kBackSwipeDistance = 50;
constexpr int kScreenSwipeDistance = 50;

void small(const char* text, int x, int y, uint32_t color = kMuted) {
  canvas.setTextColor(color);
  canvas.setFont(&fonts::Font0);
  canvas.drawString(text, x, y);
}

void formatTime(time_t value, char* text, size_t size) {
  if (value < 1704067200) {
    snprintf(text, size, "--:--");
    return;
  }
  tm local{};
  localtime_r(&value, &local);
  strftime(text, size, "%H:%M", &local);
}

void weatherIcon(int code, int x, int y, bool day = true) {
  if (code < 0 || (code > 3 && code < 45) || code > 99) {
    canvas.drawFastHLine(x - 5, y, 10, kMuted);
    return;
  }
  if (code <= 2) {
    const int sx = x - (code ? 7 : 0);
    const int sy = y - (code ? 4 : 0);
    if (day) {
      constexpr uint32_t sun = 0xFFD166;
      canvas.fillCircle(sx, sy, 9, sun);
      canvas.drawFastVLine(sx, sy - 15, 4, sun);
      canvas.drawFastVLine(sx, sy + 12, 4, sun);
      canvas.drawFastHLine(sx - 15, sy, 4, sun);
      canvas.drawFastHLine(sx + 12, sy, 4, sun);
      for (int dx : {-1, 1}) for (int dy : {-1, 1})
        canvas.drawLine(sx + dx * 9, sy + dy * 9, sx + dx * 12, sy + dy * 12, sun);
    } else {
      canvas.fillCircle(sx, sy, 10, 0xDAE6F0);
      canvas.fillCircle(sx + 5, sy - 4, 9, kCard);
    }
  }
  if (code == 0) return;

  const uint32_t cloud = code <= 3 ? 0xC8D6E5 : 0x91A4B8;
  canvas.fillCircle(x - 7, y, 6, cloud);
  canvas.fillCircle(x + 1, y - 4, 8, cloud);
  canvas.fillCircle(x + 9, y, 6, cloud);
  canvas.fillRoundRect(x - 13, y, 29, 8, 4, cloud);
  if (code == 45 || code == 48) {
    canvas.drawFastHLine(x - 15, y + 12, 30, kMuted);
    canvas.drawFastHLine(x - 11, y + 17, 25, kMuted);
  } else if (code >= 95) {
    canvas.fillTriangle(x + 1, y + 7, x + 7, y + 7, x - 4, y + 15, 0xFFD166);
    canvas.fillTriangle(x - 4, y + 13, x + 3, y + 13, x - 5, y + 20, 0xFFD166);
  } else if (code >= 51) {
    const bool snow = (code >= 71 && code <= 77) || code == 85 || code == 86;
    for (int dx = -8; dx <= 8; dx += 8) {
      if (snow) {
        canvas.drawFastHLine(x + dx - 2, y + 14, 5, kText);
        canvas.drawFastVLine(x + dx, y + 12, 5, kText);
      } else if (code <= 57) {
        canvas.fillCircle(x + dx, y + 14, 1, kAccent);
      } else {
        canvas.drawLine(x + dx + 1, y + 11, x + dx - 2, y + 17, 0x67BCFF);
        canvas.drawLine(x + dx + 2, y + 11, x + dx - 1, y + 17, 0x67BCFF);
      }
    }
  }
}

void label(const char* text, int x, int y, uint32_t color = kText) {
  canvas.setTextColor(color);
  canvas.setFont(&fonts::Font2);
  canvas.drawString(text, x, y);
}

void temperatureLabel(float temperature, bool available, int centerX, int y,
                      bool compact = false, bool large = false) {
  char text[24] = "-- C";
  if (available && std::isfinite(temperature)) {
    // Round hourly values for the narrow grid; avoid displaying negative zero.
    const float value = compact ? std::round(temperature) : temperature;
    snprintf(text, sizeof(text), compact ? "%.0f C" : "%.1f C", value == 0 ? 0.0f : value);
  }
  canvas.setFont(large ? &fonts::Font4 : &fonts::Font2);
  if (large && canvas.textWidth(text) > 92) canvas.setFont(&fonts::Font2);
  canvas.setTextColor(kText);
  canvas.drawCenterString(text, centerX, y);
}

const char* connectionLabel() {
  switch (network::status()) {
    case network::Status::Connected: return "Wi-Fi OK";
    case network::Status::Connecting: return "Wi-Fi ...";
    case network::Status::Error: return "Wi-Fi error";
    default: return "Wi-Fi unset";
  }
}

void drawHome(const environment::Snapshot& data) {
  tm now{};
  const bool clockReady = clock_service::localTime(now);
  char date[32] = "Waiting for time sync";
  char clock[16] = "--:--:--";
  if (clockReady) {
    strftime(date, sizeof(date), "%Y-%m-%d  %a", &now);
    strftime(clock, sizeof(clock), "%H:%M:%S", &now);
  }
  canvas.setFont(&fonts::Font4);
  canvas.setTextColor(kMuted);
  canvas.drawCenterString(date, 160, 12);
  canvas.setFont(&fonts::Font7);
  canvas.setTextSize(1.35f);
  canvas.setTextColor(kText);
  canvas.drawCenterString(clock, 160, 54);
  canvas.setTextSize(1);

  canvas.fillRoundRect(8, kHomeRowTop, 100, kHomeRowHeight, 8, kCard);
  canvas.fillRoundRect(kHomeWeatherLeft, kHomeRowTop, 98, kHomeRowHeight, 8, kCard);
  canvas.fillRoundRect(214, kHomeRowTop, 98, kHomeRowHeight, 8, kCard);
  small("ROOM", 16, kHomeRowTop + 5, kAccent);
  temperatureLabel(data.sensor.temperature, data.sensor.available, 58, kHomeRowTop + 22);
  char humidity[24] = "-- % RH";
  if (data.sensor.available) {
    snprintf(humidity, sizeof(humidity), "%.1f %% RH", data.sensor.humidity);
  }
  canvas.setFont(&fonts::Font2);
  canvas.setTextColor(kText);
  canvas.drawCenterString(humidity, 58, kHomeRowTop + 45);
  if (data.sensor.available && environment::sensorStale(data, millis())) small("OLD", 80, kHomeRowTop + 5, 0xFFD166);

  small("NOW", 120, kHomeRowTop + 5, kAccent);
  weatherIcon(data.weather.available ? data.weather.current.code : -1, 161, kHomeRowTop + 34, data.weather.isDay);
  temperatureLabel(data.weather.current.temperature, data.weather.available, 161, kHomeRowTop + 55);
  small("NEXT HOUR", 222, kHomeRowTop + 5, kAccent);
  const auto* next = clockReady ? environment::nextHour(data.weather, time(nullptr)) : nullptr;
  weatherIcon(next ? next->code : -1, 263, kHomeRowTop + 34, !next || next->isDay != 0);
  temperatureLabel(next ? next->temperature : NAN, next != nullptr, 263, kHomeRowTop + 55);
  if (data.weather.available && environment::weatherStale(data, millis())) {
    small("OLD", 182, kHomeRowTop + 5, 0xFFD166);
    small("OLD", 286, kHomeRowTop + 5, 0xFFD166);
  }

  char sensorTime[8];
  char weatherTime[8];
  formatTime(data.sensorUpdated, sensorTime, sizeof(sensorTime));
  formatTime(data.weatherUpdated, weatherTime, sizeof(weatherTime));
  char status[64];
  snprintf(status, sizeof(status), "%s | S %s%s | W %s%s", connectionLabel(), sensorTime,
           data.sensorError ? "!" : "", weatherTime, data.weatherError ? "!" : "");
  small(status, 12, 219);
  small("Weather: Open-Meteo.com (CC BY 4.0)", 12, 231);
}

void drawForecast(const environment::Snapshot& data) {
  label("< Back", 12, 10, kAccent);
  label("Next 24 hours", 175, 10);
  if (!forecastCount) {
    label("No forecast data", 80, 104, kMuted);
  } else {
    const size_t first = page * kHoursPerPage;
    const size_t end = first + kHoursPerPage < forecastCount ? first + kHoursPerPage : forecastCount;
    tm firstTime{}, lastTime{};
    localtime_r(&data.weather.hours[forecastIndices[first]].time, &firstTime);
    localtime_r(&data.weather.hours[forecastIndices[end - 1]].time, &lastTime);
    char from[20], to[20], range[48];
    strftime(from, sizeof(from), "%m/%d %H:%M", &firstTime);
    strftime(to, sizeof(to), "%m/%d %H:%M", &lastTime);
    snprintf(range, sizeof(range), "%s  -  %s", from, to);
    small(range, 79, 34);

    for (size_t slot = 0; slot < kHoursPerPage; ++slot) {
      const int x = 8 + (slot % 6) * 51;
      const int y = kForecastGridTop + (slot / 6) * kForecastRowHeight;
      canvas.fillRoundRect(x, y, 49, kForecastRowHeight - 2, 5, kCard);
      if (first + slot >= forecastCount) {
        weatherIcon(-1, x + 24, y + 34);
        temperatureLabel(NAN, false, x + 24, y + 55, true);
        continue;
      }
      const auto& hour = data.weather.hours[forecastIndices[first + slot]];
      char hourTime[8];
      formatTime(hour.time, hourTime, sizeof(hourTime));
      canvas.setFont(&fonts::Font0);
      canvas.setTextColor(kMuted);
      canvas.drawCenterString(hourTime, x + 24, y + 4);
      weatherIcon(hour.code, x + 24, y + 34, hour.isDay != 0);
      temperatureLabel(hour.temperature, true, x + 24, y + 55, true);
    }
    label("< Prev 12h", 12, kForecastNavTop + 3, page ? kAccent : kMuted);
    label("Next 12h >", 232, kForecastNavTop + 3, (page + 1) * kHoursPerPage < forecastCount ? kAccent : kMuted);
    char pageText[16];
    snprintf(pageText, sizeof(pageText), "%u / %u", static_cast<unsigned>(page + 1),
             static_cast<unsigned>((forecastCount + kHoursPerPage - 1) / kHoursPerPage));
    small(pageText, 143, kForecastNavTop + 8);
  }
  char updated[8];
  formatTime(data.weatherUpdated, updated, sizeof(updated));
  char status[64];
  snprintf(status, sizeof(status), "Updated %s%s", updated,
           data.weather.available && environment::weatherStale(data, millis()) ? " OLD" : "");
  small(status, 12, 220);
  small("Weather: Open-Meteo.com (CC BY 4.0)", 12, 231);
}

void drawHistoryPlot(const environment::Snapshot& data, const environment::SensorHistory& history,
                     bool temperature, int top, uint32_t nowMs) {
  constexpr int left = 46;
  constexpr int width = 260;
  constexpr int height = 46;
  const uint32_t color = temperature ? 0xFFD166 : 0x67BCFF;
  float minimum = temperature ? 20 : 40;
  float maximum = temperature ? 30 : 60;
  if (history.size()) {
    minimum = maximum = temperature ? history.at(0).temperature : history.at(0).humidity;
    for (size_t i = 1; i < history.size(); ++i) {
      const float value = temperature ? history.at(i).temperature : history.at(i).humidity;
      if (value < minimum) minimum = value;
      if (value > maximum) maximum = value;
    }
    // Keep small fluctuations readable without magnifying noise excessively.
    const float padding = temperature ? 1.0f : 5.0f;
    minimum = std::floor(minimum - padding);
    maximum = std::ceil(maximum + padding);
    if (!temperature) {
      if (minimum < 0) minimum = 0;
      if (maximum > 100) maximum = 100;
    }
  }

  small(temperature ? "TEMPERATURE / C" : "HUMIDITY / % RH", 12, top - 20, color);
  char current[24] = "--";
  if (data.sensor.available) {
    snprintf(current, sizeof(current), "%.1f%s%s",
             temperature ? data.sensor.temperature : data.sensor.humidity,
             temperature ? " C" : " %", environment::sensorStale(data, millis()) ? " OLD" : "");
  }
  canvas.setFont(&fonts::Font2);
  canvas.setTextColor(color);
  canvas.drawRightString(current, 308, top - 24);
  char limit[16];
  snprintf(limit, sizeof(limit), "%.0f", maximum);
  small(limit, 12, top - 3);
  snprintf(limit, sizeof(limit), "%.0f", minimum);
  small(limit, 12, top + height - 7);
  for (int tick = 0; tick <= 2; ++tick)
    canvas.drawFastHLine(left, top + tick * height / 2, width + 1, kCard);
  for (int tick = 0; tick <= 6; ++tick)
    canvas.drawFastVLine(left + tick * width / 6, top, height + 1, kCard);

  int previousX = 0, previousY = 0;
  bool previous = false;
  for (size_t i = 0; i < history.size(); ++i) {
    const auto& sample = history.at(i);
    const uint32_t age = nowMs - sample.receivedMs;
    if (age > environment::SensorHistory::kWindowMs) continue;
    const float value = temperature ? sample.temperature : sample.humidity;
    const int x = left + width - static_cast<uint64_t>(age) * width / environment::SensorHistory::kWindowMs;
    const int y = top + height - std::lround((value - minimum) * height / (maximum - minimum));
    if (previous && sample.connected) canvas.drawLine(previousX, previousY, x, y, color);
    canvas.fillCircle(x, y, 1, color);
    previousX = x;
    previousY = y;
    previous = true;
  }
  if (!history.size()) small("Waiting for sensor data", 89, top + 19);
}

void drawHistory(const app::State& state) {
  label("< Back", 12, 10, kAccent);
  label("Room history", 191, 10);
  const uint32_t nowMs = millis();
  drawHistoryPlot(state.data(), state.history(), true, 65, nowMs);
  drawHistoryPlot(state.data(), state.history(), false, 153, nowMs);
  small("-12h", 46, 204);
  small("-6h", 167, 204);
  small("Now", 289, 204);
  small("Last 12h | 1 min samples | since boot", 12, 225);
}
}  // namespace

bool ui::begin() {
  canvas.setColorDepth(16);
  canvas.setPsram(true);
  ready = canvas.createSprite(320, 240) != nullptr;
  if (!ready) {
    M5.Display.fillScreen(TFT_BLACK);
    M5.Display.setTextColor(TFT_WHITE);
    M5.Display.println("Display buffer unavailable");
  }
  return ready;
}

void ui::update(const app::State& state) {
  if (!ready) {
    return;
  }
  const auto& data = state.data();
  if (state.revision() != lastRevision) {
    dirty = true;
    lastRevision = state.revision();
  }
  const time_t second = time(nullptr);
  forecastCount = environment::upcomingHours(data.weather, second, forecastIndices, 24);
  if (page * kHoursPerPage >= forecastCount) page = 0;
  const auto touch = M5.Touch.getDetail();
  // Resolve taps on release so a swipe over a button cannot activate it first.
  if (touch.wasClicked()) {
    if (screen == Screen::Home && touch.x >= 8 && touch.x < 108 &&
        touch.y >= kHomeRowTop && touch.y < kHomeRowTop + kHomeRowHeight) {
      screen = Screen::History;
      dirty = true;
    } else if (screen == Screen::Home && touch.x >= kHomeWeatherLeft && touch.x < 312 &&
        touch.y >= kHomeRowTop && touch.y < kHomeRowTop + kHomeRowHeight) {
      screen = Screen::Forecast;
      page = 0;
      dirty = true;
    } else if (screen != Screen::Home && touch.x >= 0 && touch.x < 100 &&
               touch.y >= 0 && touch.y < 40) {
      screen = Screen::Home;
      dirty = true;
    } else if (screen == Screen::Forecast && touch.y >= kForecastNavTop && touch.y < kForecastNavBottom) {
      if (touch.x < 110 && page) --page;
      if (touch.x > 220 && (page + 1) * kHoursPerPage < forecastCount) ++page;
      dirty = true;
    }
  }
  if ((touch.wasFlicked() || touch.wasDragged()) &&
      touch.base_x >= 0 && touch.base_x < 320 &&
      touch.base_y >= 0 && touch.base_y < 240) {
    const int dx = touch.distanceX();
    const int dy = touch.distanceY();
    const bool fromLeftEdge = touch.base_x < kBackSwipeEdgeWidth;
    if (fromLeftEdge) {
      // Reserve the edge for Back; short/vertical gestures leave the page alone.
      if (screen != Screen::Home && dx >= kBackSwipeDistance && dx >= 2 * std::abs(dy)) {
        screen = Screen::Home;
        dirty = true;
      }
    } else if (std::abs(dx) >= kScreenSwipeDistance && std::abs(dx) >= 2 * std::abs(dy)) {
      // Cycle Home -> History -> each available forecast page -> Home.
      if (dx < 0) {
        if (screen == Screen::Home) {
          screen = Screen::History;
        } else if (screen == Screen::History) {
          screen = Screen::Forecast;
          page = 0;
        } else if ((page + 1) * kHoursPerPage < forecastCount) {
          ++page;
        } else {
          screen = Screen::Home;
        }
      } else {
        if (screen == Screen::Home) {
          screen = Screen::Forecast;
          page = forecastCount ? (forecastCount - 1) / kHoursPerPage : 0;
        } else if (screen == Screen::History) {
          screen = Screen::Home;
        } else if (page) {
          --page;
        } else {
          screen = Screen::History;
        }
      }
      dirty = true;
    } else if (screen == Screen::Forecast &&
               touch.base_y >= 40 && touch.base_y < kForecastNavTop &&
               std::abs(dy) > 30 && std::abs(dy) >= 2 * std::abs(dx)) {
      if (dy < 0 && (page + 1) * kHoursPerPage < forecastCount) ++page;
      if (dy > 0 && page) --page;
      dirty = true;
    }
  }
  const auto status = network::status();
  if (screen == Screen::Home && (second != lastSecond || status != lastStatus)) {
    dirty = true;
  }
  if (screen != Screen::Home && second / 60 != lastSecond / 60) dirty = true;
  lastSecond = second;
  lastStatus = status;
  if (!dirty) {
    return;
  }
  canvas.fillScreen(kBackground);
  if (screen == Screen::Forecast) {
    drawForecast(data);
  } else if (screen == Screen::History) {
    drawHistory(state);
  } else {
    drawHome(data);
  }
  canvas.pushSprite(0, 0);
  dirty = false;
}
