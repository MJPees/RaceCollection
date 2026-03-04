#include <Arduino.h>
#include <ArduinoWebsockets.h> //ArduinoWebsockets 0.5.4
#include <WiFi.h>
#include <Preferences.h>
#include <ArduinoJson.h>

#include "src/SD_Card.h"
#include "src/Display_ST7789.h"
#include "src/LCD_Image.h"

#ifdef local
  #undef local
#endif

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

#include <esp_mac.h>

using namespace websockets;

/* configuration */
#define PREFERENCES_NAMESPACE "starting_light" // Max 15 chars for ESP32 NVS

/* BLE configuration */
#define SERVICE_UUID           "6E400001-B5A3-F393-E0A9-E50E24DCCA9D"
#define CHARACTERISTIC_RX_UUID "6E400002-B5A3-F393-E0A9-E50E24DCCA9D"
#define CHARACTERISTIC_TX_UUID "6E400003-B5A3-F393-E0A9-E50E24DCCA9D"

#define WEBSOCKET_PING_INTERVAL 5000

#define RGB_LED_PIN 8 // Pin für RGB-LED

#define VERSION "2.0.0"
//#define DEBUG

Preferences preferences;

String currentImage = "";
String requestedImage = "/system_racingclub.png";  // soll angezeigt werden
void displayImage(const char* imagePath, bool forceUpdate = false);

String currentStatus = "idle";
unsigned long idleStartTime = 0;
unsigned long now = 0;

// display backlight control
int backlightLevel = 80;                     // Initialer Helligkeitswert
int lastButtonState = HIGH;                  // Vorheriger Tasterzustand
unsigned long lastDebounceTime = 0;
const unsigned long debounceDelay = 200;     // Entprellzeit in ms

String wifiSsid = "";
String wifiPassword = "";
String wifiHostname = "Starting-Light";

WebsocketsClient *websocketClient = nullptr;
String websocketServer = "";
unsigned long websocketLastPing = 0;
bool websocketWasConnected = false;

/* BLE variables */
const String bleName = "Starting-Light";
BLEServer* pServer = NULL;
BLECharacteristic* pTxCharacteristic = NULL;
BLECharacteristic* pRxCharacteristic = NULL;
volatile bool bleConnected = false;

String serial_usb_command_data = "";

// Pending actions deferred to loop() to avoid blocking BLE task
volatile bool pendingFinishRace  = false;
volatile bool pendingYellowFlag  = false;
volatile bool pendingRedFlag     = false;

void displayImage(const char* imagePath, bool forceUpdate) {
  if (imagePath != nullptr && imagePath[0] != '\0') {
    if (forceUpdate || requestedImage != imagePath) {
      Show_Image(imagePath);
    }

    requestedImage = imagePath;

    if (strcmp(imagePath, "/carrera_hybrid.png") == 0) {
      rgbLedWrite(RGB_LED_PIN, 0, 0, 255); // blue LED for Carrera Hybrid
    } else if (strcmp(imagePath, "/off.png") == 0) {
      rgbLedWrite(RGB_LED_PIN, 0, 0, 0); // off
    } else if (
      strcmp(imagePath, "/red_1.png") == 0 ||
      strcmp(imagePath, "/red_2.png") == 0 ||
      strcmp(imagePath, "/red_3.png") == 0 ||
      strcmp(imagePath, "/red_4.png") == 0 ||
      strcmp(imagePath, "/red_5.png") == 0
    ) {
      rgbLedWrite(RGB_LED_PIN, 0, 255, 0); // red LED for red lights
    } else if (strcmp(imagePath, "/green_5.png") == 0) {
      rgbLedWrite(RGB_LED_PIN, 255, 0, 0); // green LED for green lights
    } else if (strcmp(imagePath, "/yellow_5.png") == 0) {
      rgbLedWrite(RGB_LED_PIN, 207, 255, 0); // yellow LED for yellow lights
    } else if (
      strcmp(imagePath, "/finish_flag.png") == 0 ||
      strcmp(imagePath, "/finish.png") == 0
    ) {
      rgbLedWrite(RGB_LED_PIN, 255, 255, 255); // white LED for finish
    } else if (strcmp(imagePath, "/connect_wifi.png") == 0) {
      rgbLedWrite(RGB_LED_PIN, 255, 0, 0); // green LED for WiFi connection
    } else if (strcmp(imagePath, "/connect_websocket.png") == 0) {
      rgbLedWrite(RGB_LED_PIN, 0, 0, 255); // blue LED for websocket connection
    } else if (strcmp(imagePath, "/ap_mode.png") == 0) {
      rgbLedWrite(RGB_LED_PIN, 0, 255, 0); // red LED for AP mode
    } else {
      rgbLedWrite(RGB_LED_PIN, 0, 0, 0); // off for unknown images
    }

  } else {
    Serial.println("Error: Invalid image path provided.");
  }
}

void printCmdList() {
  Serial.println("Available Commands:");
  Serial.println(" CMD_GET_CONFIG - Get current configuration");
  Serial.println(" CMD_GET_WIFI - Get current WiFi SSID and password");
  Serial.println(" CMD_SET_WIFI:<ssid>,<password> - Set WiFi SSID and password");
  Serial.println(" CMD_GET_WEBSOCKET_SERVER - Get Websocket server URL");
  Serial.println(" CMD_SET_WEBSOCKET_SERVER:<url> - Set Websocket server URL");
  Serial.println(" CMD_SAVE_SETTINGS - Save current settings to flash");
  Serial.println(" CMD_REBOOT - Reboot device");
  Serial.println(" CMD_GET_MAC - Get BLE-MAC address");
  Serial.println(" CMD_GET_VERSION - Get firmware version");
  Serial.println(" CMD_STATUS_SET:<idle|prepare_for_start|starting|running|suspended|ended> - Set current status");
  Serial.println(" CMD_SET_IDLE - Set status to idle and show idle image");
  Serial.println(" CMD_YELLOW_FLAG - Show yellow flag image");
  Serial.println(" CMD_RED_FLAG - Show red flag image");
  Serial.println(" CMD_FINISH_RACE - Show finish race image");
  Serial.println("############################");
}

void getWifiResponse(bool fromBle = false) {
  String response = "MSG_GET_WIFI:" + wifiSsid + "," + wifiPassword;
  Serial.println(response);
  if (fromBle) {
    sendBleResponse(response);
  }
}

void getWebsocketResponse(bool fromBle = false) {
  String response = "MSG_GET_WEBSOCKET_SERVER:" + websocketServer;
  Serial.println(response);
  if (fromBle) {
    sendBleResponse(response);
  }
}

void getVersionResponse(bool fromBle = false) {
  String response = "MSG_GET_VERSION:" + String(VERSION);
  Serial.println(response);
  if (fromBle) {
    sendBleResponse(response);
  }
}

void getMacResponse(bool fromBle = false) {
  uint8_t mac[6];
  esp_read_mac(mac, ESP_MAC_BT);
  char macStr[18];
  snprintf(macStr, sizeof(macStr), "%02X%02X%02X%02X%02X%02X",
            mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  String response = "MSG_GET_MAC:" + String(macStr);
  Serial.println(response);
  if (fromBle) {
    sendBleResponse(response);
  }
}

void commandReboot(bool fromBle = false) {
  String response = "MSG_REBOOT:OK";
  Serial.println(response);
  if (fromBle) {
    sendBleResponse(response);
  }
  restartEsp32();
}

void commandGetConfig(bool fromBle = false) {
  getWifiResponse(fromBle);
  getWebsocketResponse(fromBle);
  getVersionResponse(fromBle);
  getMacResponse(fromBle);
  String response = "MSG_GET_CONFIG:OK";
  Serial.println(response);
  if (fromBle) {
    sendBleResponse(response);
  }
}

void commandSetWebsocketServer(String command, bool fromBle = false) {
  int separatorIndex = command.indexOf(':');
  String urlData = command.substring(separatorIndex + 1);
  urlData.trim();
  if (urlData.length() > 0) {
    websocketServer = urlData;
    if (websocketServer.startsWith("ws://") == false && websocketServer.startsWith("wss://") == false) {
      websocketServer = "ws://" + websocketServer;
    }
    String response = "MSG_SET_WEBSOCKET_SERVER:OK";
    Serial.println(response);
    if (fromBle) {
      sendBleResponse(response);
    }
  }
}

void commandSetWifi(String command, bool fromBle = false) {
  int separatorIndex = command.indexOf(':');
  String ssidData = command.substring(separatorIndex + 1);
  ssidData.trim();
  int commaIndex = ssidData.indexOf(',');
  if (commaIndex > 0) {
    wifiSsid = ssidData.substring(0, commaIndex);
    wifiPassword = ssidData.substring(commaIndex + 1);
    wifiSsid.trim();
    wifiPassword.trim();
  }
  else {
    wifiSsid = "";
    wifiPassword = "";
  }
  String response = "MSG_SET_WIFI:OK";
  Serial.println(response);
  if (fromBle) {
    sendBleResponse(response);
  }
}

void commandSaveSettings(bool fromBle = false) {
  configurationSave();
  String response = "MSG_SAVE_SETTINGS:OK";
  Serial.println(response);
  if (fromBle) {
    sendBleResponse(response);
  }
}

struct CountdownPatternMap {
  const char* cmd;
  const char* image;
};

const CountdownPatternMap patternMap[] = {
  {"0000000", "/off.png"},
  {"1000000", "/red_1.png"},
  {"1100000", "/red_2.png"},
  {"1110000", "/red_3.png"},
  {"1111000", "/red_4.png"},
  {"1111100", "/red_5.png"},
  {"0000010", "/green_5.png"},
  {"1111110", "/yellow_5.png"},
};

bool isValidStatus(const String& s) {
  return s == "idle" ||
         s == "prepare_for_start" ||
         s == "starting" ||
         s == "running" ||
         s == "suspended" ||
         s == "ended";
}

void commandSetStatus(String status, bool fromBle = false) {
  status.trim();
  status.toLowerCase();

  String response = "MSG_STATUS_SET:INVALID";

  if (isValidStatus(status)) {
    currentStatus = status;

    if (status == "idle") {
      displayImage("/carrera_hybrid.png");
    }

    response = "MSG_STATUS_SET:OK";
  }

  Serial.println(response);
  if (fromBle) sendBleResponse(response);
}

void commandSetIdle(bool fromBle = false) {
  commandSetStatus("idle", fromBle);
}

void commandSetCountdown(String pattern, bool fromBle = false) {
  const char* filename = "/off.png"; // default

  for (const auto& entry : patternMap) {
    if (pattern.equals(entry.cmd)) {
      filename = entry.image;
      break;
    }
  }

  if (pattern.equals("0000000")) {
    commandSetStatus("prepare_for_start", fromBle);
  } else {
    commandSetStatus("running", fromBle);
  }

  displayImage(filename, true);
  String response = "MSG_COUNTDOWN_SET:OK";
  Serial.println(response);
  if (fromBle) {
    sendBleResponse(response);
  }
}

void yellowFlag(bool fromBle = false);
void redFlag(bool fromBle = false);
void finishRace(bool fromBle = false);

using CmdHandler = void (*)(bool);

struct CmdEntry {
  const char* cmd;
  CmdHandler  fn;
};

static const CmdEntry CMD_TABLE[] = {
  {"CMD_REBOOT",        commandReboot},
  {"CMD_GET_CONFIG",    commandGetConfig},
  {"CMD_GET_WIFI",      getWifiResponse},
  {"CMD_GET_WEBSOCKET_SERVER", getWebsocketResponse},
  {"CMD_SAVE_SETTINGS", commandSaveSettings},
  {"CMD_GET_VERSION",   getVersionResponse},
  {"CMD_GET_MAC",       getMacResponse},
  {"CMD_SET_IDLE",      commandSetIdle},
  {"CMD_YELLOW_FLAG",   yellowFlag},
  {"CMD_RED_FLAG",      redFlag},
  {"CMD_FINISH_RACE",   finishRace}
};

void processCommands(String command, bool fromBle = false) {
  if (command.startsWith("CMD_SET_WIFI:")) {
    commandSetWifi(command, fromBle);
    return;
  }

  if (command.startsWith("CMD_SET_WEBSOCKET_SERVER:")) {
    commandSetWebsocketServer(command, fromBle);
    return;
  }

  if (command.startsWith("CMD_COUNTDOWN_SET:")) {
    int separatorIndex = command.indexOf(':');
    String pattern = command.substring(separatorIndex + 1);
    pattern.trim();
    commandSetCountdown(pattern, fromBle);
    return;
  }

  if (command.startsWith("CMD_STATUS_SET:")) {
    int separatorIndex = command.indexOf(':');
    String status = command.substring(separatorIndex + 1);
    status.trim();
    commandSetStatus(status, fromBle);
    return;
  }

  for (const auto& e : CMD_TABLE) {
    if (command.equalsIgnoreCase(e.cmd)) {
      e.fn(fromBle);
      return;
    }
  }

  Serial.println("Unknown command: " + command);
}

// BLE Server Callbacks
class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
      bleConnected = true;
      Serial.println("BLE: client connected");
      displayImage("/carrera_hybrid.png");
    }
    void onDisconnect(BLEServer* pServer) {
      bleConnected = false;
      Serial.println("BLE: client disconnected");
      pServer->startAdvertising(); // Restart advertising on disconnect
    }
};

class MyRxCharacteristicCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
      String rxData = pCharacteristic->getValue();
      // Validate BLE input length to prevent buffer overflow
      if (rxData.length() > 0 && rxData.length() <= 256) {
        // Process BLE command just like serial commands
        rxData.trim();
        processCommands(rxData, true);
      }
    }
};

// Function declarations

void sendBleResponse(const String &response) {
  if (bleConnected && pTxCharacteristic != NULL) {
    pTxCharacteristic->setValue(response.c_str());
    pTxCharacteristic->notify();
  }
}

void sendBleResponse(const char *response) {
  if (bleConnected && pTxCharacteristic != NULL) {
    pTxCharacteristic->setValue(response);
    pTxCharacteristic->notify();
  }
}

void configurationSave() {
  preferences.putString("wifi_ssid", wifiSsid);
  preferences.putString("wifi_password", wifiPassword);
  preferences.putString("websocket", websocketServer);
  Serial.println("Konfiguration gespeichert.");
}

void configurationLoad() {
  wifiSsid = preferences.getString("wifi_ssid", "");
  wifiPassword = preferences.getString("wifi_password", "");
  websocketServer = preferences.getString("websocket", "");
}

void wait(unsigned long waitTime) {
  unsigned long startWaitTime = millis();
  while((millis() - startWaitTime) < waitTime) {
    delay(1);
  }
}

void initBLE() {
  // Create BLE device
  BLEDevice::init(bleName.c_str());

  // Create BLE server
  pServer = BLEDevice::createServer();
  if (pServer == NULL) {
    Serial.println("BLE: ERROR - Failed to create BLE server");
    return;
  }
  pServer->setCallbacks(new MyServerCallbacks());

  // Create BLE service
  BLEService *pService = pServer->createService(SERVICE_UUID);

  // Create RX characteristic (receive from client)
  pRxCharacteristic = pService->createCharacteristic(
    CHARACTERISTIC_RX_UUID,
    BLECharacteristic::PROPERTY_WRITE
  );
  pRxCharacteristic->setCallbacks(new MyRxCharacteristicCallbacks());

  // Create TX characteristic (send to client)
  pTxCharacteristic = pService->createCharacteristic(
    CHARACTERISTIC_TX_UUID,
    BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY
  );
  pTxCharacteristic->addDescriptor(new BLE2902());

  // Start service
  pService->start();

  // Start advertising
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06);
  pAdvertising->setMaxPreferred(0x12);
  BLEDevice::startAdvertising();

  Serial.println("BLE: initialized and advertising");
}

void setup() {
  preferences.begin(PREFERENCES_NAMESPACE, false);

  Serial.begin(19200);
  wait(2000);

  pinMode(BOOT_KEY_PIN, INPUT_PULLUP);
  pinMode(RGB_LED_PIN, OUTPUT);

  LCD_Init();
  SD_Init();

  // Set initial backlight level
  Set_Backlight(backlightLevel);

  // white LED on for startup indication
  rgbLedWrite(RGB_LED_PIN, 255, 255, 255);

  Serial.print("Version: ");
  Serial.println(VERSION);
  Serial.println("############################");
  printCmdList();

  configurationLoad();

  // reserve buffer for serial commands to prevent fragmentation and ensure we can handle large commands
  serial_usb_command_data.reserve(512);

  // Init WiFi
  if (wifiSsid != "") {
    Serial.print("Connecting to WiFi SSID: ");
    Serial.println(wifiSsid);
    WiFi.setHostname(wifiHostname.c_str());
    WiFi.begin(wifiSsid.c_str(), wifiPassword.c_str());
    int wifiAttempt = 0;
    while (WiFi.status() != WL_CONNECTED && wifiAttempt < 20) {
      wait(500);
      Serial.print(".");
      wifiAttempt++;
    }
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("\nWiFi connected. IP address: ");
      Serial.println(WiFi.localIP());
      Serial.print("WiFi: Hostname: ");
      Serial.println(WiFi.getHostname());
      if (websocketServer != "") {
        bool res = connectWebsocket();
        int retry = 0;
        while (res == false && retry < 5) {
          res = connectWebsocket();
          retry += 1;
        }
      }
    } else {
      Serial.println("\nFailed to connect to WiFi.");
    }
  }

  // Initialize BLE
  initBLE();
}

void loop() {
  processSerialCommands();
  now = millis();

  if (websocketWasConnected) {
    if(websocketClient != nullptr && websocketClient->available()) {
      websocketClient->poll();
      if(now > (websocketLastPing + WEBSOCKET_PING_INTERVAL)) {
        websocketLastPing = now;
        websocketClient->ping();
      }
    }
    else if(WiFi.status() == WL_CONNECTED) {
      bool res = connectWebsocket();
      int retry = 0;
      while (res == false && retry < 5) {
        res = connectWebsocket();
        retry += 1;
      }
    }
    else {
      delete websocketClient;
      websocketClient = nullptr;
    }
  }

  // show sponsor images in idle mode
  if (bleConnected && currentStatus == "idle") {
    if (idleStartTime == 0) {
      idleStartTime = millis();
    }

    if (millis() - idleStartTime >= 30000) {
      Image_Next_Loop("/sponsor", ".png", 3000, RGB_LED_PIN);
    }
  } else {
    idleStartTime = 0;
  }

  // --- Helligkeit regeln bei Tastendruck ---
  int reading = digitalRead(BOOT_KEY_PIN);
  if (reading == LOW && lastButtonState == HIGH && (millis() - lastDebounceTime) > debounceDelay) {
    backlightLevel += 10;
    if (backlightLevel > 100) backlightLevel = 10; // Von 100 auf 10 zurückspringen
    Set_Backlight(backlightLevel);

    lastDebounceTime = millis();
  }

  // Bild nur bei Änderung anzeigen
  if (requestedImage != currentImage) {
    Show_Image(requestedImage.c_str());
    currentImage = requestedImage;
  }

  // Execute actions deferred from BLE callbacks
  if (pendingFinishRace) {
    pendingFinishRace = false;
    finishRace(false);
  } else if (pendingYellowFlag) {
    pendingYellowFlag = false;
    yellowFlag(false);
  } else if (pendingRedFlag) {
    pendingRedFlag = false;
    redFlag(false);
  }

  lastButtonState = reading;
  delay(10);
}

void processSerialCommands() {
  int len = Serial.available();
  if (len > 0) {
    if (len > 254) len = 254;
    char buffer[255];
    int bytesRead = Serial.readBytes(buffer, len);
    buffer[bytesRead] = '\0';
    serial_usb_command_data += buffer;
  }

  // Solange ein Newline gefunden wird, extrahiere und verarbeite Befehle
  int index_newline;
  while ((index_newline = serial_usb_command_data.indexOf('\n')) >= 0) {
    String command = serial_usb_command_data.substring(0, index_newline);
    serial_usb_command_data = serial_usb_command_data.substring(index_newline + 1);

    command.trim();
    if (command.length() > 0) {
      processCommands(command);
    }
  }
}

void restartEsp32() {
  wait(100);
  preferences.end();
  wait(100);
  ESP.restart();
}

bool connectWebsocket() {
  if (websocketServer == "") {
    Serial.println("Websocket: server not configured.");
    return false;
  }

  if (websocketClient) {
    delete websocketClient;
    websocketClient = nullptr;
  }
  websocketClient = new WebsocketsClient();

  websocketClient->onMessage(onMessageCallback);
  websocketClient->onEvent(onEventsCallback);

  Serial.print("Websocket: connecting ");
  Serial.println(websocketServer);

  websocketClient->connect(websocketServer);

  if(websocketClient->available()) {
    websocketClient->send("{\"type\":\"controller_set\",\"data\":{\"controller_id\":\"Z\"}}");
    return true;
  }
  delete websocketClient;
  websocketClient = nullptr;
  Serial.println("Websocket: failed to connect");
  return false;
}

void falseStart() {
  for (int i = 0; i < 2; ++i) {
    displayImage("/red_5.png", true);
    wait(300);
    displayImage("/off.png", true);
    wait(300);
  }
  displayImage("/false_start.png");
}

void stopRace() {
  displayImage("/red_5.png");
}

void yellowFlag(bool fromBle) {
  if (fromBle) {
    pendingYellowFlag = true;
    return;
  }
  for (int i = 0; i < 4; ++i) {
    if(currentStatus != "suspended") return;
    displayImage("/yellow_5.png", true);
    wait(300);
    if(currentStatus != "suspended") return;
    displayImage("/off.png", true);
    wait(300);
  }
  if(currentStatus != "suspended") return;
  displayImage("/yellow_5.png");
}

void redFlag(bool fromBle) {
  if (fromBle) {
    pendingRedFlag = true;
    return;
  }
  for (int i = 0; i < 4; ++i) {
    if(currentStatus != "suspended") return;
    displayImage("/red_5.png", true);
    wait(300);
    if(currentStatus != "suspended") return;
    displayImage("/off.png", true);
    wait(300);
  }
  if(currentStatus != "suspended") return;
  displayImage("/red_5.png");
}

void finishRace(bool fromBle) {
  if (fromBle) {
    pendingFinishRace = true;
    return;
  }
  displayImage("/finish_flag.png", true);
  wait(1000);
  displayImage("/finish.png");
}

void handleWebsocketEvent(String type, JsonDocument doc) {
  if (type == "update_event_status" && doc.containsKey("data")) {
    String data = doc["data"].as<String>();
    commandSetStatus(data); // nutzt die bestehende Logik für Statusänderungen und Bildsteuerung

    if (data == "prepare_for_start") {
      displayImage("/off.png");
    } else if (data == "starting") {
      wait(1000);
      for(int i=1; i < 6; i++) {
        String imagePath = "/red_" + String(i) + ".png";
        displayImage(imagePath.c_str());
        wait(900);
      }
    } else if (data == "running") {
      displayImage("/green_5.png");
    } else if (data == "suspended") {
      redFlag();
    } else if (data == "ended") {
      finishRace();
    } else {
      displayImage("/off.png");
    }
  } else if(type == "update_vsc_status") {
    String data = doc["data"].as<String>();
    if (data == "active") {
      commandSetStatus("suspended");
      yellowFlag();
    } else if (data == "off") {
      commandSetStatus("running");
      displayImage("/green_5.png");
    }
  } else if (type == "reset") {
    displayImage("/carrera_hybrid.png");
    commandSetStatus("idle");
  } else {
    #ifdef DEBUG
      Serial.println("Unknown message type: " + type);
    #endif
  }
}

void onMessageCallback(WebsocketsMessage message) {
  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, message.data());
  if (error) {
    Serial.print("Websocket: JSON deserialization failed: ");
    Serial.println(error.c_str());
    return;
  }

  #ifdef DEBUG
    Serial.print("Websocket: received message: ");
    serializeJsonPretty(doc, Serial);
    Serial.println();
  #endif

  if (doc.containsKey("type")) {
    handleWebsocketEvent(doc["type"].as<String>(), doc);
  } else {
    #ifdef DEBUG
      Serial.println("Received message without 'type' key: " + message.data());
    #endif
  }
}

void onEventsCallback(WebsocketsEvent event, String data) {
  if(event == WebsocketsEvent::ConnectionOpened) {
      Serial.println("Websocket: connected");
      websocketWasConnected = true;
  } else if(event == WebsocketsEvent::ConnectionClosed) {
    Serial.println("Websocket: connection closed");
    delete websocketClient;
    websocketClient = nullptr;
  } else if(event == WebsocketsEvent::GotPing) {
    if (websocketClient) websocketClient->pong();
  } else if(event == WebsocketsEvent::GotPong) {
    #ifdef DEBUG
      Serial.println("Websocket: got a pong!");
    #endif
  }
}
