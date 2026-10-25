// ECSE3038 - Tutorial 8 starting point: Tutorial 7's finished main.cpp.
// Reads a DHT22 every 10 seconds and POSTs {"mac", "temp"} to your API.
#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <DHTesp.h>
#include <ArduinoJson.h>

// Wokwi's simulated Wi-Fi: open network, no password, channel 6
#define WIFI_SSID "Wokwi-GUEST"
#define WIFI_PASSWORD ""
#define WIFI_CHANNEL 6

// Your server from Tutorial 6. http, not https, and no / at the end.
const char *API_URL = "http://iot-xackary.belgiumcentral.cloudapp.azure.com";

const unsigned long WIFI_TIMEOUT_MS = 15000;
const int DHT_PIN = 15;                       // the sensor's data wire
const unsigned long SEND_EVERY_MS = 10000;    // one reading every 10 s
const int LED_PIN = 2;          // the LED, through a 220 ohm resistor

DHTesp dht;
unsigned long lastSend = 0;
float tempLimit = NAN;          // no limit until the server gives one

bool connectWiFi() {
  Serial.print("Connecting to ");
  Serial.print(WIFI_SSID);

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD, WIFI_CHANNEL);

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED) {
    if (millis() - start > WIFI_TIMEOUT_MS) {
      Serial.println();
      Serial.println("No Wi-Fi after 15 s. Check the name and the password.");
      return false;
    }
    delay(250);
    Serial.print(".");
  }

  Serial.println(" connected");
  Serial.print("MAC address: ");
  Serial.println(WiFi.macAddress());
  return true;
}

// Part 2: the board asks your API something, like Postman does.
void checkServer() {
  HTTPClient http;
  http.begin(String(API_URL) + "/devices");
  int status = http.GET();

  Serial.print("GET /devices -> ");
  Serial.println(status);
  if (status > 0) {
    Serial.println(http.getString());
  } else {
    Serial.println(http.errorToString(status));   // no answer at all
  }
  http.end();
}

// Part 3: send one reading. The server adds the time.
void sendReading(float temp) {
  char body[80];
  snprintf(body, sizeof(body), "{\"mac\": \"%s\", \"temp\": %.1f}",
           WiFi.macAddress().c_str(), temp);

  HTTPClient http;
  http.begin(String(API_URL) + "/readings");
  http.addHeader("Content-Type", "application/json");
  int status = http.POST(body);

  Serial.print("POST ");
  Serial.print(body);
  Serial.print(" -> ");
  Serial.println(status);
  if (status == 404) {
    Serial.println("Unknown MAC: register this board with POST /devices first.");
  } else if (status < 0) {
    Serial.println(http.errorToString(status));
  }
  http.end();
}

// Ask the server for this board's limit.
// Returns the limit, or NAN (not a number) when the server gave none.
float getThreshold() {
  HTTPClient http;
  http.begin(String(API_URL) + "/settings/" + WiFi.macAddress());
  int status = http.GET();

  Serial.print("GET /settings -> ");
  Serial.println(status);
  if (status != 200) {
    if (status == 404) {
      Serial.println("No limit for this MAC yet: PUT one in /docs.");
    } else if (status < 0) {
      Serial.println(http.errorToString(status));
    }
    http.end();
    return NAN;
  }

  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, http.getString());
  http.end();
  if (error) {
    Serial.print("Could not read the JSON: ");
    Serial.println(error.c_str());
    return NAN;
  }
  return doc["temp_limit"] | NAN;   // NAN if the key is missing
}

// LED on when the temperature is above the limit.
void updateLed(float temp) {
  bool tooHot = !isnan(tempLimit) && temp > tempLimit;
  digitalWrite(LED_PIN, tooHot ? HIGH : LOW);

  Serial.print("Limit: ");
  Serial.print(tempLimit);
  Serial.print("  Temperature: ");
  Serial.print(temp);
  Serial.println(tooHot ? "  -> LED ON" : "  -> LED off");
}

void setup() {
  Serial.begin(115200);
  Serial.println("Hello from the ESP32");
  dht.setup(DHT_PIN, DHTesp::DHT22);
  pinMode(LED_PIN, OUTPUT);

  if (connectWiFi()) {
    checkServer();
  }
}

void loop() {
  if (millis() - lastSend < SEND_EVERY_MS) {
    return;
  }
  lastSend = millis();

  float temp = dht.getTemperature();
  if (isnan(temp)) {
    Serial.println("No reading from the sensor. Check the three wires.");
    return;
  }
  Serial.print("Temperature: ");
  Serial.println(temp);

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Wi-Fi dropped. Reconnecting.");
    connectWiFi();
    return;
  }
  sendReading(temp);
  tempLimit = getThreshold();
    // When the server gives no limit (404, or no answer), keep the last one.
  float newLimit = getThreshold();
  if (!isnan(newLimit)) {
    tempLimit = newLimit;
  }
  updateLed(temp);
  updateLed(temp);
}