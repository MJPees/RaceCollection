#include <Arduino.h>
#include <ArduinoWebsockets.h> //ArduinoWebsockets 0.5.4
#include <WiFi.h>
#include <Preferences.h>
#include <ArduinoJson.h>

#include "src/SD_Card.h"
#include "src/Display_ST7789.h"
#include "src/LCD_Image.h"

// Defensiv: Falls ein zuvor eingebundener Header ein "local"-Makro definiert,
// würde es die BLE-Header zerschießen – daher vor deren Include entfernen.
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
// Eigene UUID-Variante (…CCA9F) zur eindeutigen Geräte-Erkennung im Web-Client —
// RFID-Connector nutzt …CCA9E, LapTime-App …CCA9A
#define SERVICE_UUID           "6E400001-B5A3-F393-E0A9-E50E24DCCA9F"
#define CHARACTERISTIC_RX_UUID "6E400002-B5A3-F393-E0A9-E50E24DCCA9F"
#define CHARACTERISTIC_TX_UUID "6E400003-B5A3-F393-E0A9-E50E24DCCA9F"

#define WEBSOCKET_PING_INTERVAL 5000

#define RGB_LED_PIN 8 // Pin für RGB-LED

#define VERSION "2.0.0"
// #define DEBUG

Preferences preferences;

String currentImage = "";
String requestedImage = "/system_racingclub.png";  // soll angezeigt werden
void displayImage(const char* imagePath, bool forceUpdate = false);

String currentStatus = "idle";
unsigned long idleStartTime = 0;

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

String serialCommandBuffer = "";

// Pending actions deferred to loop() to avoid blocking BLE task
volatile bool pendingFinishRace  = false;
volatile bool pendingYellowFlag  = false;
volatile bool pendingRedFlag     = false;

// Logger - nutzt printf (UART0), funktioniert unabhängig vom USB-CDC-Status
void logMsg(const char* format, ...) {
  va_list args;
  va_start(args, format);
  vprintf(format, args);
  va_end(args);
  printf("\r\n");
}

// Antwort loggen und bei BLE-Herkunft zusätzlich an den Client senden
void respond(const String &response, bool fromBle) {
  logMsg("%s", response.c_str());
  if (fromBle) {
    sendBleResponse(response);
  }
}

// Liefert den Teil eines Befehls nach dem ersten ':' (getrimmt)
String commandPayload(const String &command) {
  String payload = command.substring(command.indexOf(':') + 1);
  payload.trim();
  return payload;
}

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
    logMsg("Error: Invalid image path provided.");
  }
}

void printCmdList() {
  logMsg("Available Commands:");
  logMsg(" CMD_GET_CONFIG - Get current configuration");
  logMsg(" CMD_GET_WIFI - Get current WiFi SSID and password");
  logMsg(" CMD_SET_WIFI:<ssid>,<password> - Set WiFi SSID and password");
  logMsg(" CMD_GET_WEBSOCKET_SERVER - Get Websocket server URL");
  logMsg(" CMD_SET_WEBSOCKET_SERVER:<url> - Set Websocket server URL");
  logMsg(" CMD_SAVE_SETTINGS - Save current settings to flash");
  logMsg(" CMD_REBOOT - Reboot device");
  logMsg(" CMD_GET_MAC - Get BLE-MAC address");
  logMsg(" CMD_GET_VERSION - Get firmware version");
  logMsg(" CMD_STATUS_SET:<idle|prepare_for_start|starting|running|suspended|ended> - Set current status");
  logMsg(" CMD_COUNTDOWN_SET:<pattern> - Set countdown lights, 7-digit pattern (e.g. 1111100)");
  logMsg(" CMD_SET_IDLE - Set status to idle and show idle image");
  logMsg(" CMD_YELLOW_FLAG - Show yellow flag image");
  logMsg(" CMD_RED_FLAG - Show red flag image");
  logMsg(" CMD_FINISH_RACE - Show finish race image");
  logMsg("############################");
}

void sendWifiResponse(bool fromBle = false) {
  respond("MSG_GET_WIFI:" + wifiSsid + "," + wifiPassword, fromBle);
}

void sendWebsocketResponse(bool fromBle = false) {
  respond("MSG_GET_WEBSOCKET_SERVER:" + websocketServer, fromBle);
}

void sendVersionResponse(bool fromBle = false) {
  respond("MSG_GET_VERSION:" + String(VERSION), fromBle);
}

void sendMacResponse(bool fromBle = false) {
  uint8_t mac[6];
  esp_read_mac(mac, ESP_MAC_BT);
  char macStr[18];
  snprintf(macStr, sizeof(macStr), "%02X%02X%02X%02X%02X%02X",
            mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  respond("MSG_GET_MAC:" + String(macStr), fromBle);
}

void commandReboot(bool fromBle = false) {
  respond("MSG_REBOOT:OK", fromBle);
  restartEsp32();
}

void commandGetConfig(bool fromBle = false) {
  sendWifiResponse(fromBle);
  sendWebsocketResponse(fromBle);
  sendVersionResponse(fromBle);
  sendMacResponse(fromBle);
  respond("MSG_GET_CONFIG:OK", fromBle);
}

void commandSetWebsocketServer(String urlData, bool fromBle = false) {
  if (urlData.length() > 0) {
    websocketServer = urlData;
    if (websocketServer.startsWith("ws://") == false && websocketServer.startsWith("wss://") == false) {
      websocketServer = "ws://" + websocketServer;
    }
    respond("MSG_SET_WEBSOCKET_SERVER:OK", fromBle);
  } else {
    respond("MSG_SET_WEBSOCKET_SERVER:INVALID", fromBle);
  }
}

void commandSetWifi(String ssidData, bool fromBle = false) {
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
  respond("MSG_SET_WIFI:OK", fromBle);
}

void commandSaveSettings(bool fromBle = false) {
  configurationSave();
  respond("MSG_SAVE_SETTINGS:OK", fromBle);
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

  respond(response, fromBle);
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
  respond("MSG_COUNTDOWN_SET:OK", fromBle);
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
  {"CMD_GET_WIFI",      sendWifiResponse},
  {"CMD_GET_WEBSOCKET_SERVER", sendWebsocketResponse},
  {"CMD_SAVE_SETTINGS", commandSaveSettings},
  {"CMD_GET_VERSION",   sendVersionResponse},
  {"CMD_GET_MAC",       sendMacResponse},
  {"CMD_SET_IDLE",      commandSetIdle},
  {"CMD_YELLOW_FLAG",   yellowFlag},
  {"CMD_RED_FLAG",      redFlag},
  {"CMD_FINISH_RACE",   finishRace}
};

void processCommands(String command, bool fromBle = false) {
  if (command.startsWith("CMD_SET_WIFI:")) {
    commandSetWifi(commandPayload(command), fromBle);
    return;
  }

  if (command.startsWith("CMD_SET_WEBSOCKET_SERVER:")) {
    commandSetWebsocketServer(commandPayload(command), fromBle);
    return;
  }

  if (command.startsWith("CMD_COUNTDOWN_SET:")) {
    commandSetCountdown(commandPayload(command), fromBle);
    return;
  }

  if (command.startsWith("CMD_STATUS_SET:")) {
    commandSetStatus(commandPayload(command), fromBle);
    return;
  }

  for (const auto& e : CMD_TABLE) {
    if (command.equalsIgnoreCase(e.cmd)) {
      e.fn(fromBle);
      return;
    }
  }

  logMsg("Unknown command: %s", command.c_str());
}

// BLE Server Callbacks
class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
      bleConnected = true;
      logMsg("BLE: client connected");
    }
    void onDisconnect(BLEServer* pServer) {
      bleConnected = false;
      logMsg("BLE: client disconnected");
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

void configurationSave() {
  preferences.putString("wifi_ssid", wifiSsid);
  preferences.putString("wifi_password", wifiPassword);
  preferences.putString("websocket", websocketServer);
  logMsg("Konfiguration gespeichert.");
}

void configurationLoad() {
  logMsg("Konfiguration laden...");
  wifiSsid = preferences.getString("wifi_ssid", "");
  wifiPassword = preferences.getString("wifi_password", "");
  websocketServer = preferences.getString("websocket", "");
  logMsg("WiFi SSID: %s", wifiSsid.c_str());
  logMsg("Websocket Server: %s", websocketServer.c_str());
}

void initBLE() {
  // Create BLE device
  BLEDevice::init(bleName.c_str());

  // Create BLE server
  pServer = BLEDevice::createServer();
  if (pServer == NULL) {
    logMsg("BLE: ERROR - Failed to create BLE server");
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

  logMsg("BLE: initialized and advertising");
  displayImage("/carrera_hybrid.png");
}

void setup() {
  preferences.begin(PREFERENCES_NAMESPACE, false);

  Serial.begin(115200);
  // Warten bis Serial-Monitor verbunden ist (max. 5 Sek.), damit keine DBG-Meldungen verloren gehen
  { unsigned long _t = millis(); while (!Serial && (millis() - _t) < 5000) delay(10); }
  delay(200);

  pinMode(BOOT_KEY_PIN, INPUT_PULLUP);
  pinMode(RGB_LED_PIN, OUTPUT);

  logMsg("LCD_Init...");
  LCD_Init();
  logMsg("LCD_Init done");

  SD_Init();
  logMsg("SD_Init done");

  Set_Backlight(backlightLevel);
  rgbLedWrite(RGB_LED_PIN, 255, 255, 255);

  logMsg("Version: %s", VERSION);
  logMsg("############################");
  printCmdList();

  configurationLoad();

  // reserve buffer for serial commands to prevent fragmentation and ensure we can handle large commands
  serialCommandBuffer.reserve(512);

  // Init WiFi
  if (wifiSsid != "") {
    displayImage("/connect_wifi.png");
    logMsg("Connecting to WiFi SSID: %s", wifiSsid.c_str());
    WiFi.setHostname(wifiHostname.c_str());
    WiFi.begin(wifiSsid.c_str(), wifiPassword.c_str());
    int wifiAttempt = 0;
    while (WiFi.status() != WL_CONNECTED && wifiAttempt < 20) {
      delay(500);
      printf(".");
      wifiAttempt++;
    }
    if (WiFi.status() == WL_CONNECTED) {
      logMsg("\r\nWiFi connected. IP: %s  Hostname: %s",
             WiFi.localIP().toString().c_str(), WiFi.getHostname());
      if (websocketServer != "") {
        connectWebsocketWithRetry();
      }
    } else {
      logMsg("Failed to connect to WiFi.");
    }
  }

  // Initialize BLE
  initBLE();
}

void loop() {
  processSerialCommands();
  unsigned long now = millis();

  if (websocketWasConnected) {
    if(websocketClient != nullptr && websocketClient->available()) {
      websocketClient->poll();
      if(now > (websocketLastPing + WEBSOCKET_PING_INTERVAL)) {
        websocketLastPing = now;
        websocketClient->ping();
      }
    }
    else if(WiFi.status() == WL_CONNECTED) {
      connectWebsocketWithRetry();
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
    serialCommandBuffer += buffer;
  }

  // Solange ein Newline gefunden wird, extrahiere und verarbeite Befehle
  int index_newline;
  while ((index_newline = serialCommandBuffer.indexOf('\n')) >= 0) {
    String command = serialCommandBuffer.substring(0, index_newline);
    serialCommandBuffer = serialCommandBuffer.substring(index_newline + 1);

    command.trim();
    if (command.length() > 0) {
      processCommands(command);
    }
  }
}

void restartEsp32() {
  delay(100);
  preferences.end();
  delay(100);
  ESP.restart();
}

bool connectWebsocket() {
  displayImage("/connect_websocket.png");
  logMsg("Websocket: connect ...");

  if (websocketServer == "") {
    logMsg("Websocket: server not configured.");
    return false;
  }

  if (websocketClient) {
    delete websocketClient;
    websocketClient = nullptr;
  }
  websocketClient = new WebsocketsClient();

  websocketClient->onMessage(onMessageCallback);
  websocketClient->onEvent(onEventsCallback);

  logMsg("Websocket: connecting %s", websocketServer.c_str());

  websocketClient->connect(websocketServer);

  if(websocketClient->available()) {
    logMsg("Websocket: connected");
    websocketClient->send("{\"type\":\"controller_set\",\"data\":{\"controller_id\":\"Z\"}}");
    logMsg("Websocket: sent initial controller_set message");
    return true;
  }
  delete websocketClient;
  websocketClient = nullptr;
  logMsg("Websocket: failed to connect");
  return false;
}

bool connectWebsocketWithRetry() {
  for (int attempt = 0; attempt < 6; ++attempt) {
    if (connectWebsocket()) {
      return true;
    }
  }
  return false;
}

void yellowFlag(bool fromBle) {
  if (fromBle) {
    pendingYellowFlag = true;
    return;
  }
  for (int i = 0; i < 4; ++i) {
    if(currentStatus != "suspended") return;
    displayImage("/yellow_5.png", true);
    delay(300);
    if(currentStatus != "suspended") return;
    displayImage("/off.png", true);
    delay(300);
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
    delay(300);
    if(currentStatus != "suspended") return;
    displayImage("/off.png", true);
    delay(300);
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
  delay(1000);
  displayImage("/finish.png");
}

void handleWebsocketEvent(String type, JsonDocument doc) {
  if (type == "update_event_status" && doc.containsKey("data")) {
    String data = doc["data"].as<String>();
    commandSetStatus(data); // nutzt die bestehende Logik für Statusänderungen und Bildsteuerung

    if (data == "prepare_for_start") {
      displayImage("/off.png");
    } else if (data == "starting") {
      delay(1000);
      for(int i=1; i < 6; i++) {
        String imagePath = "/red_" + String(i) + ".png";
        displayImage(imagePath.c_str());
        delay(900);
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
      logMsg("Unknown message type: %s", type.c_str());
    #endif
  }
}

void onMessageCallback(WebsocketsMessage message) {
  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, message.data());
  if (error) {
    logMsg("Websocket: JSON deserialization failed: %s", error.c_str());
    return;
  }

  #ifdef DEBUG
    String _jsonDbg;
    serializeJsonPretty(doc, _jsonDbg);
    logMsg("Websocket: received message: %s", _jsonDbg.c_str());
  #endif

  if (doc.containsKey("type")) {
    handleWebsocketEvent(doc["type"].as<String>(), doc);
  } else {
    #ifdef DEBUG
      logMsg("Received message without 'type' key: %s", message.data().c_str());
    #endif
  }
}

void onEventsCallback(WebsocketsEvent event, String data) {
  if(event == WebsocketsEvent::ConnectionOpened) {
      logMsg("Websocket: connected");
      websocketWasConnected = true;
  } else if(event == WebsocketsEvent::ConnectionClosed) {
    logMsg("Websocket: connection closed");
    delete websocketClient;
    websocketClient = nullptr;
  } else if(event == WebsocketsEvent::GotPing) {
    if (websocketClient) websocketClient->pong();
  } else if(event == WebsocketsEvent::GotPong) {
    #ifdef DEBUG
      logMsg("Websocket: got a pong!");
    #endif
  }
}
