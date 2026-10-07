#include "network.h"

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ESPmDNS.h>
#include <esp_sntp.h>
#include <freertos/queue.h>
#include <atomic>
#include <cstring>
#include <memory>
#include <new>

#include "app_config.h"
#include "weather_ca.h"

namespace {
std::atomic<network::Status> connection{network::Status::Unconfigured};
std::atomic<bool> timeSynced{false};
QueueHandle_t updates = nullptr;

class LocalClient : public WiFiClient {
 public:
  using WiFiClient::connect;
  int connect(const char* hostname, uint16_t port, int32_t timeout) override {
    String host(hostname);
    if (!host.endsWith(".local")) return WiFiClient::connect(hostname, port, timeout);
    host.remove(host.length() - 6);
    const IPAddress address = MDNS.queryHost(host, 2000);
    if (address == IPAddress()) return 0;
    // Resolve only the transport address; HTTP keeps its original Host header.
    return WiFiClient::connect(address, port, timeout);
  }
};

class ResponseBuffer : public Stream {
 public:
  static constexpr size_t kCapacity = 16384;
  std::unique_ptr<char[]> bytes{new (std::nothrow) char[kCapacity + 1]};
  size_t size = 0;
  size_t write(uint8_t value) override { return write(&value, 1); }
  size_t write(const uint8_t* data, size_t length) override {
    if (!bytes || length > kCapacity - size) return 0;
    std::memcpy(bytes.get() + size, data, length);
    size += length;
    bytes[size] = '\0';
    return length;
  }
  int available() override { return 0; }
  int read() override { return -1; }
  int peek() override { return -1; }
  void flush() override {}
};

bool fetch(String url, ResponseBuffer& body) {
  if (!body.bytes) return false;
  LocalClient plain;
  WiFiClientSecure secure;
  HTTPClient http;
  const bool tls = url.startsWith("https://");
  if (!tls && !url.startsWith("http://")) return false;

  if (tls) {
    secure.setCACert(kWeatherRootCa);
    secure.setHandshakeTimeout(8);
  }
  WiFiClient& client = tls ? static_cast<WiFiClient&>(secure) : static_cast<WiFiClient&>(plain);
  if (!http.begin(client, url)) return false;
  http.setConnectTimeout(5000);
  http.setTimeout(5000);
  http.setReuse(false);
  const int status = http.GET();
  bool ok = status == HTTP_CODE_OK && http.getSize() <= static_cast<int>(ResponseBuffer::kCapacity);
  if (ok) ok = http.writeToStream(&body) > 0;
  http.end();
  if (!ok) Serial.printf("HTTP request failed (status %d)\n", status);
  return ok;
}

bool updateSensor(environment::Sensor& sensor) {
  ResponseBuffer body;
  return fetch(config::kSensorEndpoint, body) &&
         environment::parseSensor(body.bytes.get(), body.size, sensor);
}

bool updateWeather(environment::Weather& weather) {
  char url[512];
  snprintf(url, sizeof(url),
           "https://api.open-meteo.com/v1/forecast?latitude=%.7f&longitude=%.7f"
           "&current=temperature_2m,weather_code,is_day"
           "&hourly=temperature_2m,weather_code,precipitation_probability,is_day"
           "&forecast_days=2&timezone=Asia%%2FTokyo&timeformat=unixtime",
           config::kWeatherLatitude, config::kWeatherLongitude);
  ResponseBuffer body;
  return fetch(url, body) && environment::parseWeather(body.bytes.get(), body.size, weather);
}

void onTimeSync(struct timeval*) {
  // The callback runs in the network stack. RTC/I2C stays on the UI task.
  timeSynced.store(true);
}

void run(void*) {
  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(config::kWifiSsid, config::kWifiPassword);
  uint32_t lastAttempt = millis();
  bool ntpStarted = false;
  bool mdnsStarted = false;
  bool sensorAttempted = false;
  bool weatherAttempted = false;
  uint32_t sensorAttempt = 0;
  uint32_t weatherAttempt = 0;
  uint32_t weatherDelay = config::kWeatherRefreshMs;
  environment::Snapshot snapshot;

  for (;;) {
    const bool connected = WiFi.status() == WL_CONNECTED;
    connection.store(connected ? network::Status::Connected
                               : network::Status::Connecting);
    if (connected && !ntpStarted) {
      sntp_set_time_sync_notification_cb(onTimeSync);
      configTzTime(config::kTimezone, config::kNtpServer1, config::kNtpServer2);
      ntpStarted = true;
    }
    if (connected && !mdnsStarted) mdnsStarted = MDNS.begin("m5core2-clock");
    if (!connected && millis() - lastAttempt >= config::kWifiRetryMs) {
      WiFi.reconnect();
      lastAttempt = millis();
    }
    if (connected && config::kSensorEndpoint[0] &&
        (!sensorAttempted || millis() - sensorAttempt >= config::kSensorRefreshMs)) {
      snapshot.sensorError = !updateSensor(snapshot.sensor);
      sensorAttempt = millis();
      sensorAttempted = true;
      if (!snapshot.sensorError) {
        snapshot.sensorUpdated = time(nullptr);
        snapshot.sensorReceivedMs = millis();
      }
      Serial.println(snapshot.sensorError ? "Sensor update failed" : "Sensor updated");
      xQueueOverwrite(updates, &snapshot);
    }
    // TLS certificate validation needs a sensible clock (RTC or NTP).
    if (connected && config::kWeatherLocationConfigured && time(nullptr) >= 1704067200 &&
        (!weatherAttempted || millis() - weatherAttempt >= weatherDelay)) {
      snapshot.weatherError = !updateWeather(snapshot.weather);
      weatherAttempt = millis();
      weatherAttempted = true;
      weatherDelay = snapshot.weatherError ? config::kWeatherRetryMs : config::kWeatherRefreshMs;
      if (!snapshot.weatherError) {
        snapshot.weatherUpdated = time(nullptr);
        snapshot.weatherReceivedMs = millis();
      }
      Serial.println(snapshot.weatherError ? "Weather update failed" : "Weather updated");
      xQueueOverwrite(updates, &snapshot);
    }
    // Display, touch and RTC/I2C remain exclusively on the main task.
    vTaskDelay(pdMS_TO_TICKS(250));
  }
}
}  // namespace

void network::begin() {
  if (config::kWifiSsid[0] == '\0') {
    return;
  }
  updates = xQueueCreate(1, sizeof(environment::Snapshot));
  if (!updates) {
    connection.store(Status::Error);
    return;
  }
  connection.store(Status::Connecting);
  if (xTaskCreate(run, "network", 12288, nullptr, 1, nullptr) != pdPASS) {
    vQueueDelete(updates);
    updates = nullptr;
    connection.store(Status::Error);
    Serial.println("Failed to create network task");
  }
}

network::Status network::status() {
  return connection.load();
}

bool network::consumeTimeSync() {
  return timeSynced.exchange(false);
}

bool network::receive(environment::Snapshot& snapshot) {
  return updates && xQueueReceive(updates, &snapshot, 0) == pdTRUE;
}
