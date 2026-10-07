#include "clock_service.h"

#include <M5Unified.h>
#include <cstdlib>
#include <sys/time.h>

#include "app_config.h"
#include "network.h"

namespace {
bool valid = false;
}  // namespace

void clock_service::begin() {
  // RTC stores UTC. Set TZ before converting the RTC calendar to system time.
  setenv("TZ", "UTC0", 1);
  tzset();
  m5::rtc_datetime_t rtc;
  if (M5.Rtc.isEnabled() && !M5.Rtc.getVoltLow() && M5.Rtc.getDateTime(&rtc) &&
      rtc.date.year >= 2024 && rtc.date.year <= 2099 &&
      rtc.date.month >= 1 && rtc.date.month <= 12 &&
      rtc.date.date >= 1 && rtc.date.date <= 31 &&
      rtc.time.hours < 24 && rtc.time.minutes < 60 && rtc.time.seconds < 60) {
    M5.Rtc.setSystemTimeFromRtc();
    valid = true;
    Serial.println("Clock restored from RTC (UTC)");
  }
  setenv("TZ", config::kTimezone, 1);
  tzset();
}

void clock_service::update() {
  if (!network::consumeTimeSync()) {
    return;
  }
  valid = true;
  if (M5.Rtc.isEnabled()) {
    const time_t now = time(nullptr);
    tm utc{};
    gmtime_r(&now, &utc);
    M5.Rtc.setDateTime(&utc);
  }
  Serial.println("NTP synchronized");
}

bool clock_service::localTime(tm& result) {
  if (!valid) {
    return false;
  }
  const time_t now = time(nullptr);
  return localtime_r(&now, &result) != nullptr;
}
