#include "BLEDevice.h"
#include "HeltecLCD.h"
#include "Arduino.h"
#include <WiFi.h>
#include "ESP32MQTTClient.h"
// Documentation
// https://www.electronics-lab.com/project/using-the-ble-functionality-of-the-esp32/

/*
 * Constants
 */
const int SERIAL_BAUD = 115200;

const char *BLE_ESP32_NAME = "esp32-olivier_ph-ble";  // TODO: replace with your name

const char *BLE_HM10_MAC = "A4:06:E9:8D:ED:E9";       // TODO: replace with HM-10 MAC
static BLEUUID BLE_HM10_SERVICE_UUID("ff13");         // TODO: replace with HM-10 ffXX
static BLEUUID BLE_HM10_CHARACTERISTIC_UUID("ffe1");  // Do not modify, default HM-10 characteristic
const unsigned long BLE_HM10_RECONNECT_DELAY = 10000;

const char *BLE_GROVE_NAME = "grove-13-ble";  // TODO: replace with grove-XX-ble
const unsigned long BLE_GROVE_RECONNECT_DELAY = 10000;


const char *WIFI_SSID = "oph2";            // "wifimael";
const char *WIFI_PASSWORD = "1nf1cvte2f";  // "ja6z04xady";
const char *DEVICE_HOSTNAME = "esp32-olivier_ph-ble";


// URI with local hostname must end with .local or it will not be reachable
const char *MQTT_URI = "mqtt://192.168.62.200:1883";  // "mqtt://192.168.99.104:1883";
const char *MQTT_LAST_WILL_TOPIC = "home/esp32";
const char *MQTT_LAST_WILL_MSG = "OFFLINE";
const int MQTT_KEEP_ALIVE = 30;

const String MQTT_TOPIC_HOME_ALL = "home/#";

const String MQTT_TOPIC_HOME_ACTION_OL = "home/actionOl";
const String MQTT_PUBLISH_OUTSIDE_LIGHT = "home/outsideLight";
const String MQTT_PUBLISH_INSIDE_LIGHT = "home/insideLight";




/*
 * Variables
 */
HeltecLCD heltecLCD;
BLERemoteCharacteristic *pRemoteCharacteristic = nullptr;
bool isHM10Connected = false;
bool isGroveBLEConnected = false;
unsigned long nextHM10ConnectionMillis = 0;
unsigned long nextGroveBLEConnectionMillis = 0;
ESP32MQTTClient mqttClient;
int pubCount = 0;

/*
 * Declarations
 */
bool connectHM10();

/*
 * Functions
 */
void setup() {
  // Begin as Bluetooth master

  Serial.begin(SERIAL_BAUD);
  Serial.println(F("BLUETOOTH STARTED"));
  heltecLCD.begin();


  BLEDevice::init(BLE_ESP32_NAME);
  mqttClient.setURI(MQTT_URI);
  mqttClient.setKeepAlive(MQTT_KEEP_ALIVE);
  mqttClient.enableDebuggingMessages();
  mqttClient.enableLastWillMessage(MQTT_LAST_WILL_TOPIC, MQTT_LAST_WILL_MSG);
}

static void notifyCallback(
  BLERemoteCharacteristic *pBLERemoteCharacteristic,
  uint8_t *pData,
  size_t length,
  bool isNotify) {
  String receivedString((char *)pData, length);
  receivedString.trim();
  Serial.print(F("Received: '"));
  Serial.print(receivedString);
  Serial.println(F("'"));  // Print the string in quotes for clarity

  // Check if it matches
  if (receivedString == "ols") {
    Serial.println(F("Matched 'ols'"));
    mqttClient.publish(MQTT_TOPIC_HOME_ACTION_OL, receivedString);
    return;
  }
}

bool connectHM10() {
  Serial.println(F("HM-10 : trying to connect"));
  BLEClient *pClient = BLEDevice::createClient();
  pClient->disconnect();

  bool isConnected = pClient->connect(BLEAddress(BLE_HM10_MAC));
  if (!isConnected) {
    Serial.println(F("HM-10 : connection failed"));
    return false;
  }

  Serial.println(F("HM-10 : connected"));

  BLERemoteService *pRemoteService = pClient->getService(BLE_HM10_SERVICE_UUID);
  if (pRemoteService == nullptr) {
    Serial.print(F("HM-10 : service UUID not found "));
    Serial.println(BLE_HM10_SERVICE_UUID.toString().c_str());
    return false;
  }

  Serial.print(F("HM-10 : service UUID connected "));
  Serial.println(BLE_HM10_SERVICE_UUID.toString().c_str());

  pRemoteCharacteristic = pRemoteService->getCharacteristic(BLE_HM10_CHARACTERISTIC_UUID);
  if (!pRemoteCharacteristic->canNotify()) {
    pRemoteCharacteristic = nullptr;
    Serial.println(F("HM-10 : cannot rewrite char UUID"));
    return false;
  }

  pRemoteCharacteristic->registerForNotify(notifyCallback);
  Serial.println(F("HM-10 : communication ready"));
  return true;
}

void loop() {

  if (!isHM10Connected && millis() >= nextHM10ConnectionMillis) {
    isHM10Connected = connectHM10();
    nextHM10ConnectionMillis = millis() + BLE_HM10_RECONNECT_DELAY;

    if (isHM10Connected)
      heltecLCD.print("CONNECTED ");
  }



  // Read data from Serial Monitor
  if (Serial.available()) {
    String data = Serial.readStringUntil('\n');

    if (isHM10Connected && pRemoteCharacteristic) {
      pRemoteCharacteristic->writeValue(data.c_str(), data.length());
      Serial.print(F("HM-10 sending : "));
      Serial.println(data);
    }
  }



  if (WiFi.status() != WL_CONNECTED) {
    // Tries to connect to the Wifi
    WiFi.disconnect();
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    WiFi.setHostname(DEVICE_HOSTNAME);

    // Wait until Wifi is connected to prevent losing messages or doing work that requires Wifi
    Serial.print("CONNECTING");
    heltecLCD.clear();
    heltecLCD.print("CONNECTING");

    while (WiFi.status() != WL_CONNECTED) {
      Serial.print(".");
      heltecLCD.print(".");
      delay(100);
    }

    // Wifi is connected, display IP and start MQTT client
    IPAddress ip = WiFi.localIP();  // IPAddress = int[]
    String ipString = String(ip[0]) + "." + String(ip[1]) + "." + String(ip[2]) + "." + String(ip[3]);

    Serial.println();
    Serial.println("CONNECTED IP : ");
    Serial.println(ipString);

    heltecLCD.clear();
    heltecLCD.println("CONNECTED IP : ");
    heltecLCD.println(ipString);

    // MQTT loops by itself once start and sub/pub MQTT messages
    mqttClient.loopStart();
  }



  delay(2000);
}

void onMqttHomeAll(const String &topic, const String &payload) {
  String message = topic + " " + payload;
  Serial.println(message);
  heltecLCD.println(message);
  if (topic == MQTT_PUBLISH_OUTSIDE_LIGHT) {
    if (payload == "false") {
      message = "olf";
    } else {
      message = "olt";
    }
  }
  if (topic == MQTT_TOPIC_HOME_ACTION_OL) message = "";

  if (topic == MQTT_PUBLISH_INSIDE_LIGHT) {
    message = payload;
  }
  pRemoteCharacteristic->writeValue(message.c_str(), message.length());
}



void onMqttConnect(esp_mqtt_client_handle_t client) {
  if (!mqttClient.isMyTurn(client))  // can be omitted if only one client
    return;

  // mqttClient.subscribe(MQTT_TOPIC_HOME_TEST, [](const String &payload) { onMqttHomeTest(payload); });


  // Example to subscribe to all messages with a prefix (ex. home/#)
  mqttClient.subscribe(MQTT_TOPIC_HOME_ALL, [](const String &topic, const String &payload) {
    onMqttHomeAll(topic, payload);
  });
}

void handleMQTT(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
  auto *event = static_cast<esp_mqtt_event_handle_t>(event_data);
  mqttClient.onEventCallback(event);
}