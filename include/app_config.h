#pragma once

#include <cstdint>

// Generated from environment variables by PlatformIO before compilation.
#if __has_include("clock_build_config.h")
#include "clock_build_config.h"
#endif

#if __has_include("secrets.h")
#include "secrets.h"
#endif

#ifndef CLOCK_WIFI_SSID
#define CLOCK_WIFI_SSID ""
#endif
#ifndef CLOCK_WIFI_PASSWORD
#define CLOCK_WIFI_PASSWORD ""
#endif
#ifndef CLOCK_SENSOR_ENDPOINT
#define CLOCK_SENSOR_ENDPOINT ""
#endif

#if defined(CLOCK_WEATHER_LATITUDE) != defined(CLOCK_WEATHER_LONGITUDE)
#error "Set both CLOCK_WEATHER_LATITUDE and CLOCK_WEATHER_LONGITUDE"
#endif

namespace config {
constexpr char kWifiSsid[] = CLOCK_WIFI_SSID;
constexpr char kWifiPassword[] = CLOCK_WIFI_PASSWORD;
constexpr char kSensorEndpoint[] = CLOCK_SENSOR_ENDPOINT;
#if defined(CLOCK_WEATHER_LATITUDE) && defined(CLOCK_WEATHER_LONGITUDE)
constexpr bool kWeatherLocationConfigured = true;
constexpr double kWeatherLatitude = CLOCK_WEATHER_LATITUDE;
constexpr double kWeatherLongitude = CLOCK_WEATHER_LONGITUDE;
static_assert(kWeatherLatitude >= -90.0 && kWeatherLatitude <= 90.0,
              "Weather latitude must be between -90 and 90");
static_assert(kWeatherLongitude >= -180.0 && kWeatherLongitude <= 180.0,
              "Weather longitude must be between -180 and 180");
#else
constexpr bool kWeatherLocationConfigured = false;
constexpr double kWeatherLatitude = 0.0;
constexpr double kWeatherLongitude = 0.0;
#endif
// POSIX TZ uses the opposite sign to a UTC offset: JST is UTC+9.
constexpr char kTimezone[] = "JST-9";
constexpr char kNtpServer1[] = "ntp.nict.jp";
constexpr char kNtpServer2[] = "pool.ntp.org";
constexpr uint32_t kWifiRetryMs = 30000;
constexpr uint32_t kSensorRefreshMs = 30000;
constexpr uint32_t kWeatherRefreshMs = 15 * 60 * 1000;
constexpr uint32_t kWeatherRetryMs = 60000;
constexpr uint32_t kSensorStaleMs = 120000;
constexpr uint32_t kWeatherStaleMs = 30 * 60 * 1000;
constexpr uint8_t kBrightness = 128;
}  // namespace config
