
#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <DHT.h>
#include "secrets.h"

// ---------------- HARDWARE ----------------

#define DHT_PIN 15
#define LED_PIN 2
#define DHT_TYPE DHT22

DHT dht(DHT_PIN, DHT_TYPE);

// ---------------- MQTT BROKER ----------------

const char* MQTT_BROKER = "135.232.203.144";
const int MQTT_PORT = 1883;

const char* MQTT_CLIENT_ID = "fan3038-device";

String settingsTopic = "ecse3038/fan3038/fan/settings";
String stateTopic = "ecse3038/fan3038/fan/state";
String statusTopic = "ecse3038/fan3038/status";

// Separate network clients
WiFiClient mqttWifiClient;
WiFiClient httpWifiClient;

PubSubClient mqtt(mqttWifiClient);

// ---------------- FAN SETTINGS ----------------

String fanMode = "off";
float maxHumidity = 65.0;
float currentHumidity = -1;

bool settingsReceived = false;

bool fanIsOn = false;
bool statePublished = false;
bool lastPublishedState = false;

// ---------------- TIMING ----------------

unsigned long lastReadingTime = 0;
const unsigned long readingInterval = 10000;

// ---------------- FUNCTION DECLARATIONS ----------------

void connectWiFi();
void connectMQTT();
void mqttCallback(char* topic, byte* payload, unsigned int length);
void sendReading(float humidity);
void applyFanSettings();
void publishFanState();

// ---------------- WIFI CONNECTION ----------------

void connectWiFi() {
  if (WiFi.status() == WL_CONNECTED) {
    return;
  }

  Serial.println("Connecting to WiFi...");

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.println("WiFi connected.");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

// ---------------- MQTT CALLBACK ----------------

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String message = "";

  for (unsigned int i = 0; i < length; i++) {
    message += (char)payload[i];
  }

  Serial.print("Received MQTT message: ");
  Serial.println(message);

  JsonDocument doc;

  DeserializationError error = deserializeJson(doc, message);

  // Reject invalid JSON
  if (error) {
    Serial.println("Invalid JSON. Settings unchanged.");
    return;
  }

  // Check mode field
  if (!doc["mode"].is<const char*>()) {
    Serial.println("Invalid or missing mode.");
    return;
  }

  // Check humidity field
  if (!doc["max_humidity"].is<float>() &&
      !doc["max_humidity"].is<int>()) {
    Serial.println("Invalid or missing max_humidity.");
    return;
  }

  String newMode = doc["mode"].as<String>();
  float newMaxHumidity = doc["max_humidity"].as<float>();

  // Validate supported modes
  if (newMode != "auto" &&
      newMode != "on" &&
      newMode != "off") {
    Serial.println("Unsupported mode. Settings unchanged.");
    return;
  }

  // Validate humidity range
  if (newMaxHumidity < 0 || newMaxHumidity > 100) {
    Serial.println("Humidity must be between 0 and 100.");
    return;
  }

  // Accept valid settings
  fanMode = newMode;
  maxHumidity = newMaxHumidity;
  settingsReceived = true;

  Serial.println("Settings accepted.");
  Serial.print("Mode: ");
  Serial.println(fanMode);
  Serial.print("Maximum humidity: ");
  Serial.println(maxHumidity);

  // Apply settings immediately
  applyFanSettings();
}

// ---------------- MQTT CONNECTION ----------------

void connectMQTT() {
  while (!mqtt.connected()) {

    Serial.println("Connecting to MQTT...");

    bool connected = mqtt.connect(
      MQTT_CLIENT_ID,
      statusTopic.c_str(),
      0,
      true,
      "offline"
    );

    if (connected) {

      Serial.println("MQTT connected.");

      // Publish online status with retention
      mqtt.publish(statusTopic.c_str(), "online", true);

      // Subscribe to fan settings
      mqtt.subscribe(settingsTopic.c_str());

      Serial.println("Subscribed to fan settings.");

      // Republish current state after reconnection
      statePublished = false;

    } else {

      Serial.print("MQTT connection failed. State: ");
      Serial.println(mqtt.state());

      delay(2000);
    }
  }
}

// ---------------- SEND HUMIDITY TO API ----------------

void sendReading(float humidity) {

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi unavailable. Reading not sent.");
    return;
  }

  HTTPClient http;

  String url = String(API_URL) + "/readings";

  http.begin(httpWifiClient, url);

  http.addHeader("Content-Type", "application/json");
  http.addHeader("X-API-Key", DEVICE_KEY);

  JsonDocument doc;

  doc["mac"] = DEVICE_MAC;
  doc["humidity"] = humidity;

  String requestBody;

  serializeJson(doc, requestBody);

  int responseCode = http.POST(requestBody);

  Serial.print("API response: ");
  Serial.println(responseCode);

  if (responseCode > 0) {
    Serial.println(http.getString());
  } else {
    Serial.println("API request failed.");
  }

  http.end();
}

// ---------------- FAN CONTROL ----------------

void applyFanSettings() {

  bool desiredState = false;

  // Default fan state is OFF until settings arrive
  if (settingsReceived) {

    if (fanMode == "on") {
      desiredState = true;
    }

    else if (fanMode == "off") {
      desiredState = false;
    }

    else if (fanMode == "auto") {

      if (currentHumidity >= 0) {
        desiredState = currentHumidity > maxHumidity;
      }
    }
  }

  // Change LED only when fan state changes
  if (desiredState != fanIsOn) {

    fanIsOn = desiredState;

    digitalWrite(
      LED_PIN,
      fanIsOn ? HIGH : LOW
    );

    Serial.print("Fan state changed: ");
    Serial.println(fanIsOn ? "ON" : "OFF");
  }

  publishFanState();
}

// ---------------- PUBLISH FAN STATE ----------------

void publishFanState() {

  if (!mqtt.connected()) {
    return;
  }

  // Publish only when state changes or after MQTT reconnect
  if (!statePublished || fanIsOn != lastPublishedState) {

    const char* stateMessage = fanIsOn ? "on" : "off";

    bool published = mqtt.publish(
      stateTopic.c_str(),
      stateMessage,
      true
    );

    if (published) {

      lastPublishedState = fanIsOn;
      statePublished = true;

      Serial.print("Published fan state: ");
      Serial.println(stateMessage);
    }
  }
}

// ---------------- SETUP ----------------

void setup() {

  Serial.begin(115200);

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  dht.begin();

  connectWiFi();

  mqtt.setServer(MQTT_BROKER, MQTT_PORT);
  mqtt.setCallback(mqttCallback);

  Serial.println("Bathroom fan controller started.");
}

// ---------------- MAIN LOOP ----------------

void loop() {

  // Maintain WiFi connection
  if (WiFi.status() != WL_CONNECTED) {
    connectWiFi();
  }

  // Maintain MQTT connection
  if (!mqtt.connected()) {
    connectMQTT();
  }

  // Process incoming MQTT messages
  mqtt.loop();

  // Read and upload humidity every 10 seconds
  if (millis() - lastReadingTime >= readingInterval) {

    lastReadingTime = millis();

    float humidity = dht.readHumidity();

    if (!isnan(humidity)) {

      currentHumidity = humidity;

      Serial.print("Humidity: ");
      Serial.print(currentHumidity);
      Serial.println("%");

      sendReading(currentHumidity);

      // Recalculate fan state using latest humidity
      applyFanSettings();

    } else {

      Serial.println("DHT22 reading failed.");
    }
  }

  // Keep fan control responsive
  applyFanSettings();

  delay(50);
}
