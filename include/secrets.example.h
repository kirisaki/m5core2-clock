#pragma once

// Local build defaults. Environment variables take precedence.
#ifndef CLOCK_WIFI_SSID
#define CLOCK_WIFI_SSID ""
#endif
#ifndef CLOCK_WIFI_PASSWORD
#define CLOCK_WIFI_PASSWORD ""
#endif
#ifndef CLOCK_SENSOR_ENDPOINT
#define CLOCK_SENSOR_ENDPOINT "http://sensor.local/api/v1/environment"
#endif

// Optional weather location: uncomment both blocks and enter your coordinates.
// #ifndef CLOCK_WEATHER_LATITUDE
// #define CLOCK_WEATHER_LATITUDE 35.0
// #endif
// #ifndef CLOCK_WEATHER_LONGITUDE
// #define CLOCK_WEATHER_LONGITUDE 140.0
// #endif
