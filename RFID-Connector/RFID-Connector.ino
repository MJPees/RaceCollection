#include <Arduino.h>
#include <ArduinoWebsockets.h> //ArduinoWebsockets 0.5.4
#include <WiFi.h>
#include <Preferences.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <esp_mac.h>

using namespace websockets;

/* configuration */
#define RFID_DEFAULT_POWER_LEVEL 26 // default power level
#define DEFAULT_MIN_LAP_TIME 3000 //min time between laps in ms
#define PREFERENCES_NAMESPACE "rfid_connector" // Max 15 chars for ESP32 NVS

/* BLE configuration */
#define SERVICE_UUID           "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_RX_UUID "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_TX_UUID "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"

#define WEBSOCKET_PING_INTERVAL 5000

#define VERSION "2.0.0"
//#define DEBUG

//#define ESP32C3
#define ESP32DEV

#ifdef ESP32DEV
  #define SerialRFID Serial2
  #define RX_PIN 17
  #define TX_PIN 16
  #define RFID_LED_PIN 32
  #define BLE_LED_PIN 33
  #define WEBSOCKET_LED_PIN 25
  #define PUSH_BUTTON_PIN 23
#elif defined(ESP32C3)
  HardwareSerial SerialRFID(1);
  #define RX_PIN 5
  #define TX_PIN 6
  #define RFID_LED_PIN 8
  #define BLE_LED_PIN 9
  #define WEBSOCKET_LED_PIN 10
#endif

#define RFID_LED_ON_TIME 200

#define RFID_RESTART_TIME 300000
#define RFID_MAX_COUNT 30 // max für freies Fahren laut Carrera
#define RFID_MAX_MAPPING_COUNT 30 // max number of tag to id mappings

#define debounceDelay 50

const String bleName = "RFID-Connector";

const unsigned char ReadMulti[10] = {0XAA,0X00,0X27,0X00,0X03,0X22,0XFF,0XFF,0X4A,0XDD};
const unsigned char StopReadMultiResponse[8] = {0xAA,0x01,0x28,0x00,0x01,0x00,0x2A,0xDD};
const unsigned char StopReadMulti[7] = {0XAA,0X00,0X28,0X00,0X00,0X28,0XDD};
const unsigned char Power10dbm[9] = {0XAA,0X00,0XB6,0X00,0X02,0X03,0XE8,0XA3,0XDD};
const unsigned char Power11dbm[9] = {0XAA,0X00,0XB6,0X00,0X02,0X04,0X4C,0X08,0XDD};
const unsigned char Power12dbm[9] = {0XAA,0X00,0XB6,0X00,0X02,0X04,0XB0,0X6C,0XDD};
const unsigned char Power13dbm[9] = {0XAA,0X00,0XB6,0X00,0X02,0X05,0X14,0XD1,0XDD};
const unsigned char Power14dbm[9] = {0XAA,0X00,0XB6,0X00,0X02,0X05,0X78,0X35,0XDD};
const unsigned char Power15dbm[9] = {0XAA,0X00,0XB6,0X00,0X02,0X05,0XDC,0X99,0XDD};
const unsigned char Power16dbm[9] = {0XAA,0X00,0XB6,0X00,0X02,0X06,0X40,0XFE,0XDD};
const unsigned char Power17dbm[9] = {0XAA,0X00,0XB6,0X00,0X02,0X06,0XA4,0X62,0XDD};
const unsigned char Power18dbm[9] = {0XAA,0X00,0XB6,0X00,0X02,0X07,0X08,0XC7,0XDD};
const unsigned char Power19dbm[9] = {0XAA,0X00,0XB6,0X00,0X02,0X07,0X6C,0X2B,0XDD};
const unsigned char Power20dbm[9] = {0XAA,0X00,0XB6,0X00,0X02,0X07,0XD0,0X8F,0XDD};
const unsigned char Power21dbm[9] = {0XAA,0X00,0XB6,0X00,0X02,0X08,0X34,0XF4,0XDD};
const unsigned char Power22dbm[9] = {0XAA,0X00,0XB6,0X00,0X02,0X08,0X98,0X58,0XDD};
const unsigned char Power23dbm[9] = {0XAA,0X00,0XB6,0X00,0X02,0X08,0XFC,0XBC,0XDD};
const unsigned char Power24dbm[9] = {0XAA,0X00,0XB6,0X00,0X02,0X09,0X60,0X21,0XDD};
const unsigned char Power25dbm[9] = {0XAA,0X00,0XB6,0X00,0X02,0X09,0XC4,0X85,0XDD};
const unsigned char Power26dbm[9] = {0XAA,0X00,0XB6,0X00,0X02,0X0A,0X28,0XEA,0XDD};
const unsigned char PowerLevelResponse[] = {0xAA,0x01,0xB6,0x00,0x01,0x00,0xB8,0xDD};
const unsigned char Europe[8] = {0XAA,0X00,0X07,0X00,0X01,0X03,0X0B,0XDD};
const unsigned char RegionResponse[] = {0xAA,0x01,0x07,0x00,0x01,0x00,0x09,0xDD};
const unsigned char HighSensitivity[8] = {0XAA,0X00,0XF5,0X00,0X01,0X00,0XF6,0XDD};
const unsigned char HighSensitivityResponse[8] = {0XAA,0X01,0XF5,0X00,0X01,0X00,0XF7,0XDD};
const unsigned char DenseReader[8] = {0XAA,0X00,0XF5,0X00,0X01,0X01,0XF7,0XDD};
const unsigned char DenseReaderResponse[8] = {0XAA,0X01,0XF5,0X00,0X01,0X00,0XF7,0XDD};
const unsigned char NoModuleSleepTime[8] = {0XAA,0X00,0X1D,0x00,0x01,0x00,0x1E,0xDD};
const unsigned char NoModuleSleepTimeResponse[] = {0XAA,0X01,0X1D,0x00,0x01,0x00,0x1F,0xDD};

unsigned int rfidSerialByte = 0;
bool startByte = false;
bool gotMessageType = false;
unsigned char messageType = 0;
unsigned char command = 0;
unsigned int rssi = 0;
unsigned int pc = 0;
unsigned int parameterLength = 0;
unsigned int crc = 0;
unsigned int dataCheckSum = 0;

unsigned char epcBytes[12] = {};
String lastEpcString = "";
unsigned long lastEpcRead = 0;
unsigned long lastRestart = 0;
unsigned long RfidLedOnMs = 0;
unsigned long lastButtonChange = 0;
unsigned long now = 0;
bool readTag = false;
bool buttonWasPressed = false;

int minLapTime = DEFAULT_MIN_LAP_TIME;

Preferences preferences;

int rfidPowerLevel = RFID_DEFAULT_POWER_LEVEL;
bool rfidDenseMode = true;

struct mapping_data {
  char tagId[32] = "";
  char mappedId[32] = "";
};

mapping_data mappings[RFID_MAX_MAPPING_COUNT];

struct rfid_data {
  String tagId;
  String mappedId = "";
  unsigned long last;
};
rfid_data rfids[RFID_MAX_COUNT];

String wifiSsid = "";
String wifiPassword = "";
String wifiHostname = "RFID-Connector";

WebsocketsClient *websocketClient = nullptr;
String websocketServer = "";
unsigned long websocketLastPing = 0;
bool websocketWasConnected = false;

/* BLE variables */
BLEServer* pServer = NULL;
BLECharacteristic* pTxCharacteristic = NULL;
BLECharacteristic* pRxCharacteristic = NULL;
volatile bool bleConnected = false;

String serial_usb_command_data = "";

void ledOn(const int led_pin) {
  digitalWrite(led_pin, LOW);
  if(led_pin == RFID_LED_PIN) {
    RfidLedOnMs = millis();
  }
}

void ledOff(const int led_pin) {
  digitalWrite(led_pin, HIGH);
  if(led_pin == RFID_LED_PIN) {
    RfidLedOnMs = 0;
  }
}

void clearMapping() {
  for (int i = 0; i < RFID_MAX_MAPPING_COUNT; i++) {
    mappings[i].tagId[0] = '\0';
    mappings[i].mappedId[0] = '\0';
  }
}

void printCmdList() {
  Serial.println("Available Commands:");
  Serail.println(" CMD_GET_CONFIG - Get current configuration");
  Serial.println(" CMD_GET_DENSE_MODE - Get current RFID dense mode setting");
  Serial.println(" CMD_SET_DENSE_MODE:<0|1> - Set RFID dense mode (0=off, 1=on)");
  Serial.println(" CMD_GET_POWER - Get current RFID power level");
  Serial.println(" CMD_SET_POWER:<level> - Set RFID power level (10-26 dBm)");
  Serial.println(" CMD_GET_MIN_LAP_TIME - Get minimum lap time setting");
  Serial.println(" CMD_SET_MIN_LAP_TIME:<ms> - Set minimum lap time in milliseconds");
  Serial.println(" CMD_SET_MAPPING:<tag_id>,<mapped_id|controller_id> - Set mapping from tag ID to mapped ID or controller ID");
  Serial.println(" CMD_REMOVE_MAPPING:<tag_id> - Remove mapping for specified tag ID");
  Serial.println(" CMD_GET_MAPPINGS - Get all current tag ID to mapped ID mappings");
  Serial.println(" CMD_CLEAR_MAPPINGS - Clear all tag ID to mapped ID mappings");
  Serial.println(" CMD_GET_WIFI - Get current WiFi SSID and password");
  Serial.println(" CMD_SET_WIFI:<ssid>,<password> - Set WiFi SSID and password");
  Serial.println(" CMD_GET_WEBSOCKET_SERVER - Get Websocket server URL");
  Serial.println(" CMD_SET_WEBSOCKET_SERVER:<url> - Set Websocket server URL");
  Serial.println(" CMD_SAVE_SETTINGS - Save current settings to flash");
  Serial.println(" CMD_RESET_RFID_STORAGE - Reset RFID tag storage");
  Serial.println(" CMD_WRITE_RFID:<[1:255]> - Write new RFID tag with specified ID (last byte)");
  Serial.println(" CMD_REBOOT - Reboot device");
  Serial.println(" CMD_GET_VERSION - Get firmware version");
  Serial.println("############################");
}

void getDenseModeResponse(bool fromBle = false) {
  String response = rfidDenseMode ? "MSG_GET_DENSE_MODE:1" : "MSG_GET_DENSE_MODE:0";
  Serial.println(response);
  if(fromBle) {
    sendBleResponse(response);
  }
}

void getPowerLevelResponse(bool fromBle = false) {
  String response = "MSG_GET_POWER:" + String(rfidPowerLevel);
  Serial.println(response);
  if(fromBle) {
    sendBleResponse(response);
  }
}

void getMinLapTimeResponse(bool fromBle = false) {
  String response = "MSG_GET_MIN_LAP_TIME:" + String(minLapTime);
  Serial.println(response);
  if(fromBle) {
    sendBleResponse(response);
  }
}

void getMappingResponse(bool fromBle = false) {
  for (int i = 0; i < RFID_MAX_COUNT; i++) {
    if(rfids[i].tagId != "" && rfids[i].mappedId == "") {
      String response = "MSG_MAPPING:" + rfids[i].tagId + "," + rfids[i].tagId;
      Serial.println(response);
      if (fromBle) {
        sendBleResponse(response);
        wait(50); // small wait to ensure BLE notifications are sent in order
      }
    }
  }
  for (int i = 0; i < RFID_MAX_MAPPING_COUNT; i++) {
    if (mappings[i].tagId[0] != '\0') {
      char buffer[128]; // Groß genug für Befehl + Tag + Map
      snprintf(buffer, sizeof(buffer), "MSG_GET_MAPPING:%s,%s", mappings[i].tagId, mappings[i].mappedId);
      String response = String(buffer);
      Serial.println(response);
      if (fromBle) {
        sendBleResponse(response);
        wait(50); // small wait to ensure BLE notifications are sent in order
      }
    }
  }
  String response = "MSG_GET_MAPPINGS_END:OK";
  Serial.println(response);
  if (fromBle) {
    sendBleResponse(response);
  }
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

void processCommands(String command, bool fromBle = false) {
  if (command.equalsIgnoreCase("CMD_GET_DENSE_MODE")) {
    getDenseModeResponse(fromBle);
  }
  else if (command.indexOf("CMD_SET_DENSE_MODE:") >= 0) {
    int separatorIndex = command.indexOf(':');
    String denseModeString = command.substring(separatorIndex + 1);
    denseModeString.trim();
    if (denseModeString.equalsIgnoreCase("1")) {
      rfidSetDensitivityMode(true);
    } else {
      rfidSetDensitivityMode(false);
    }
    String response = "MSG_SET_DENSE_MODE:OK";
    Serial.println(response);
    if (fromBle) {
      sendBleResponse(response);
    }
  }
  else if (command.equalsIgnoreCase("CMD_GET_POWER")) {
    getPowerLevelResponse(fromBle);
  }
  else if (command.indexOf("CMD_SET_POWER:") >= 0) {
    int separatorIndex = command.indexOf(':');
    String powerLevelString = command.substring(separatorIndex + 1);
    powerLevelString.trim();
    rfidSetPowerLevel(powerLevelString.toInt());
    String response = "MSG_SET_POWER:OK";
    Serial.println(response);
    if (fromBle && bleConnected && pTxCharacteristic != NULL) {
      pTxCharacteristic->setValue(response.c_str());
      pTxCharacteristic->notify();
    }
  }
  else if (command.equalsIgnoreCase("CMD_GET_MIN_LAP_TIME")) {
    getMinLapTimeResponse(fromBle);
  }  
  else if (command.indexOf("CMD_SET_MIN_LAP_TIME:") >= 0) {
    int separatorIndex = command.indexOf(':');
    String minLapTimeString = command.substring(separatorIndex + 1);
    minLapTimeString.trim();
    minLapTime = minLapTimeString.toInt();
    String response = "MSG_SET_MIN_LAP_TIME:OK";
    Serial.println(response);
    if (fromBle) {
      sendBleResponse(response);
    }
  }
  else if (command.equalsIgnoreCase("CMD_REBOOT")) {
    String response = "MSG_REBOOT:OK";
    Serial.println(response);
    if (fromBle) {
      sendBleResponse(response);
    }
    restartEsp32();
  }
  else if(command.indexOf("CMD_SET_MAPPING:") >= 0) {
    int separatorIndex = command.indexOf(':');
    String mappingData = command.substring(separatorIndex + 1);
    mappingData.trim();
    int commaIndex = mappingData.indexOf(',');
    if (commaIndex > 0) {
      String tagId = mappingData.substring(0, commaIndex);
      String mappedId = mappingData.substring(commaIndex + 1);
      tagId.trim();
      mappedId.trim();
      // Use optimized search to find or update mapping
      int mappingIdx = findMappingByTagId(tagId);
      if (mappingIdx >= 0) {
        // Mapping exists, update it
        strncpy(mappings[mappingIdx].mappedId, mappedId.c_str(), sizeof(mappings[mappingIdx].mappedId) - 1);
        // WICHTIG: Manuell Null-Terminator setzen, falls der String zu lang war
        mappings[mappingIdx].mappedId[sizeof(mappings[mappingIdx].mappedId) - 1] = '\0';
      } else {
        // Mapping doesn't exist, find empty slot
        int emptyIdx = findFirstEmptyMapping();
        if (emptyIdx >= 0) {
          strncpy(mappings[emptyIdx].tagId, tagId.c_str(), sizeof(mappings[emptyIdx].tagId) - 1);
          // WICHTIG: Manuell Null-Terminator setzen, falls der String zu lang war
          mappings[emptyIdx].tagId[sizeof(mappings[emptyIdx].tagId) - 1] = '\0';
          strncpy(mappings[emptyIdx].mappedId, mappedId.c_str(), sizeof(mappings[emptyIdx].mappedId) - 1);
          // WICHTIG: Manuell Null-Terminator setzen, falls der String zu lang war
          mappings[emptyIdx].mappedId[sizeof(mappings[emptyIdx].mappedId) - 1] = '\0';
        }
      }
      String response = "MSG_SET_MAPPING:OK";
      Serial.println(response);
      if (fromBle) {
        sendBleResponse(response);
      }
    }
  }
  else if(command.indexOf("CMD_REMOVE_MAPPING:") >= 0) {
    int separatorIndex = command.indexOf(':');
    String tagId = command.substring(separatorIndex + 1);
    tagId.trim();
    int mappingIdx = findMappingByTagId(tagId);
    if (mappingIdx >= 0) {
      mappings[mappingIdx].tagId[0] = '\0';
      mappings[mappingIdx].mappedId[0] = '\0';
    }
    String response = "MSG_REMOVE_MAPPING:OK";
    Serial.println(response);
    if (fromBle) {
      sendBleResponse(response);
    }
  }
  else if(command.equalsIgnoreCase("CMD_GET_MAPPINGS")) {
    getMappingResponse(fromBle);
  }
  else if(command.equalsIgnoreCase("CMD_CLEAR_MAPPINGS")) {
    clearMapping();
    String response = "MSG_CLEAR_MAPPINGS:OK";
    Serial.println(response);
    if (fromBle) {
      sendBleResponse(response);
    }
  }
  else if(command.equalsIgnoreCase("CMD_GET_WIFI")) {
    getWifiResponse(fromBle);
  }
  else if(command.indexOf("CMD_SET_WIFI:") >= 0) {
    int separatorIndex = command.indexOf(':');
    String ssidData = command.substring(separatorIndex + 1);
    ssidData.trim();
    int commaIndex = ssidData.indexOf(',');
    if (commaIndex > 0) {
      wifiSsid = ssidData.substring(0, commaIndex);
      wifiPassword = ssidData.substring(commaIndex + 1);
      wifiSsid.trim();
      wifiPassword.trim();
      // Here you would save the SSID and password to preferences or a config file
      String response = "MSG_SET_WIFI:OK";
      Serial.println(response);
      if (fromBle) {
        sendBleResponse(response);
      }
    }
  }
  else if(command.equalsIgnoreCase("CMD_GET_WEBSOCKET_SERVER")) {
    getWebsocketResponse(fromBle);
  }
  else if(command.indexOf("CMD_SET_WEBSOCKET_SERVER:") >= 0) {
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
  else if(command.equalsIgnoreCase("CMD_RESET_RFID_STORAGE")) {
    resetRfidStorage();
    String response = "MSG_RESET_RFID_STORAGE:OK";
    Serial.println(response);
    if (fromBle) {
      sendBleResponse(response);
    }
  }
  else if(command.indexOf("CMD_WRITE_RFID:") >= 0) {
    int separatorIndex = command.indexOf(':');
    String rfidId = command.substring(separatorIndex + 1);
    rfidId.trim();
    int lastByte = rfidId.toInt();
    if (lastByte < 0| lastByte > 255) {
      String response = "MSG_WRITE_RFID:ERROR";
      Serial.println(response);
      if (fromBle) {
        sendBleResponse(response);
      }
      return;
    }
    setReaderSetting(StopReadMulti, 7, StopReadMultiResponse, 8);
    wait(100);
    bool writeResult = writeRfidEpc(lastByte);
    String response = writeResult ? "MSG_WRITE_RFID:OK" : "MSG_WRITE_RFID:ERROR";
    Serial.println(response);
    if (fromBle) {
      sendBleResponse(response);
    }
    SerialRFID.write(ReadMulti,10);
  }
  else if(command.equalsIgnoreCase("CMD_SAVE_SETTINGS")) {
    configurationSave();
    String response = "MSG_SAVE_SETTINGS:OK";
    Serial.println(response);
    if (fromBle) {
      sendBleResponse(response);
    }
  }
  else if(command.equalsIgnoreCase("CMD_GET_CONFIG")) {
    getWifiResponse(fromBle);
    getWebsocketResponse(fromBle);
    getDenseModeResponse(fromBle);
    getPowerLevelResponse(fromBle);
    getMinLapTimeResponse(fromBle);
    getMappingResponse(fromBle);
    getVersionResponse(fromBle);
    String response = "MSG_GET_CONFIG:OK";
    Serial.println(response);
    if (fromBle) {
      sendBleResponse(response);
    }
  }
  else if(command.equalsIgnoreCase("CMD_GET_VERSION")) {
    getVersionResponse(fromBle);
  }
  else {
    Serial.println("Unknown command: " + command);
  }
}

// BLE Server Callbacks
class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
      bleConnected = true;
      Serial.println("BLE: client connected");
      ledOn(BLE_LED_PIN);
    }
    void onDisconnect(BLEServer* pServer) {
      bleConnected = false;
      Serial.println("BLE: client disconnected");
      ledOff(BLE_LED_PIN);
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

int findMappingByTagId(const String &tagId) {
  for (int i = 0; i < RFID_MAX_MAPPING_COUNT; i++) {
    // strcmp gibt 0 zurück, wenn beide Zeichenketten identisch sind
    if (strcmp(mappings[i].tagId, tagId.c_str()) == 0) {
      return i;
    }
  }
  return -1;
}

int findFirstEmptyMapping() {
  for (int i = 0; i < RFID_MAX_MAPPING_COUNT; i++) {
    // Ein Slot ist leer, wenn das erste Zeichen der Null-Terminator ist
    if (mappings[i].tagId[0] == '\0') {
      return i;
    }
  }
  return -1;
}

void configurationSave() {
  preferences.putString("wifi_ssid", wifiSsid);
  preferences.putString("wifi_password", wifiPassword);
  preferences.putString("websocket", websocketServer);
  preferences.putBool("rfid_dense_mode", rfidDenseMode);
  preferences.putInt("power_level", rfidPowerLevel);
  preferences.putInt("min_lap_time", minLapTime);
  preferences.putBytes("map_block", mappings, sizeof(mappings));
  Serial.println("Konfiguration und alle Mappings wurden gespeichert.");
}

void configurationLoad() {
  wifiSsid = preferences.getString("wifi_ssid", "");
  wifiPassword = preferences.getString("wifi_password", "");
  websocketServer = preferences.getString("websocket", "");
  rfidDenseMode = preferences.getBool("rfid_dense_mode", true);
  rfidPowerLevel = preferences.getInt("power_level", RFID_DEFAULT_POWER_LEVEL);
  minLapTime = preferences.getInt("min_lap_time", DEFAULT_MIN_LAP_TIME);
  size_t storedSize = preferences.getBytesLength("map_block");
  if (storedSize == sizeof(mappings)) {
    preferences.getBytes("map_block", mappings, sizeof(mappings));
    Serial.println("RFID Mappings erfolgreich als Block geladen.");
  } else {
    // Falls noch nichts gespeichert wurde oder die Größe nicht passt: Arrays leeren
    clearMapping();
    Serial.println("Keine gültigen Mappings gefunden, Speicher initialisiert.");
  }
}

void sendFinishLineMessage(int controller_id, unsigned long timestamp, String rfidString) {
  // set last timestamp for the controller
  rfids[controller_id-1].last = timestamp;

  bool send_ok = false;
  // Use snprintf to avoid String memory fragmentation
  char message[64];
  snprintf(message, sizeof(message), "%s#%lu", rfidString.c_str(), timestamp);
  Serial.println(message);
  // Send via BLE if connected
  if (bleConnected && pTxCharacteristic != NULL) {
    sendBleResponse(message);
    send_ok = true;
  }
  if(websocketClient != nullptr && websocketClient->available()) {
      char wsMessage[128];
      if (rfidString.length() == 1) {
        // If rfidString is mappid_id and is a single character, treat it as controller ID
        int ctrl_id = rfidString.toInt();
        snprintf(wsMessage, sizeof(wsMessage), "{\"type\":\"analog_lap\",\"data\":{\"timestamp\":%lu,\"controller_id\":\"%d\"}}", timestamp, ctrl_id);
      } else {
        snprintf(wsMessage, sizeof(wsMessage), "{\"type\":\"analog_lap\",\"data\":{\"timestamp\":%lu,\"controller_id\":\"%d\"}}", timestamp, controller_id);
      }
      websocketClient->send(wsMessage);
      send_ok = true;
  }
  if(send_ok) {
    ledOn(RFID_LED_PIN);
  } else {
    ledOff(RFID_LED_PIN);
  }
}

void sendFinishLineEvent(String rfidString, unsigned long ms) {
  for (int i = 0; i < RFID_MAX_COUNT; i++) {
    if(rfids[i].tagId == rfidString) {
      if(rfids[i].last + minLapTime < ms) {
        if (rfids[i].mappedId != "") {
          sendFinishLineMessage(i+1, ms, rfids[i].mappedId);
        }
        else {
          sendFinishLineMessage(i+1, ms, rfidString);
        }
      }
      return;
    }
  }
  // New RFID tag
  for(int i=0; i < RFID_MAX_COUNT; i++) {
    if(rfids[i].tagId == "") {
      rfids[i].tagId = rfidString;
      Serial.print("RFID: new car at controller id: ");
      Serial.print(i+1);
      Serial.print(" => ");
      Serial.print(rfids[i].tagId);
      Serial.println();
      // Use optimized search for mapping
      int mappingIdx = findMappingByTagId(rfidString);
      if (mappingIdx >= 0) {
        rfids[i].mappedId = mappings[mappingIdx].mappedId;
        Serial.print(" -> mapped to id: ");
        Serial.print(rfids[i].mappedId);
        Serial.println();
        sendFinishLineMessage(i+1, ms, mappings[mappingIdx].mappedId);
      } else {
        Serial.println();
        sendFinishLineMessage(i+1, ms, rfidString);
      }
      return;
    }
  }
}

void wait(unsigned long waitTime) {
  unsigned long startWaitTime = millis();
  while((millis() - startWaitTime) < waitTime) {
    delay(1);
  }
}

void resetRfidStorage() {
  now = millis();
  for(int i=0; i < RFID_MAX_COUNT; i++) {
    rfids[i].tagId = "";
    rfids[i].mappedId = "";
    rfids[i].last = now;
  }
}

bool checkResponse(const unsigned char expectedBuffer[], int length) {
  bool ok = true;
  unsigned char buffer[length];
  SerialRFID.readBytes(buffer, length);
  #ifdef DEBUG
    Serial.println("\nResponse:\n");
  #endif
  for (int i = 0; i < length; ++i) {
    #ifdef DEBUG
      Serial.print(" 0x");
      Serial.print(buffer[i], HEX);
    #endif
    if (buffer[i] != expectedBuffer[i]) {
      ok = false;
    }
  }
  #ifdef DEBUG
    Serial.println("\nExpected Response:\n");
    for (int i = 0; i < length; ++i) {
        Serial.print(" 0x");
        Serial.print(expectedBuffer[i], HEX);
    }
    Serial.println("");
  #endif
  return ok;
}

bool setReaderSetting(const unsigned char sendBuffer[], int sendLength, const unsigned char expectedResponseBuffer[], int expectedLength) {
  bool ok = false;
  int retries = 0;
  while (!ok && retries < 3) {
    while(SerialRFID.available()) {
      SerialRFID.read();
    }
    SerialRFID.write(sendBuffer, sendLength);
    ok = checkResponse(expectedResponseBuffer, expectedLength);
    retries++;
  }
  return ok;
}

void rfidSetDensitivityMode(bool dense_mode) {
  // Set reader mode based on loaded configuration
  bool ok;

  if(dense_mode) {
    ok = setReaderSetting(DenseReader, 8, DenseReaderResponse, 8);
    if(ok) {
      Serial.println("RFID: set dense reader mode.");
      ledOn(RFID_LED_PIN);
      wait(200);
      ledOff(RFID_LED_PIN);
      wait(100);
      ledOn(RFID_LED_PIN);
      wait(200);
      ledOff(RFID_LED_PIN);
      wait(200);
      rfidDenseMode = true;
    } else {
      Serial.println("RFID: failed to set dense reader mode.");
    }
  } else {
    ok = setReaderSetting(HighSensitivity, 8, HighSensitivityResponse, 8);
    if(ok) {
      Serial.println("RFID: set high sensitivity reader mode.");
      ledOn(RFID_LED_PIN);
      wait(200);
      ledOff(RFID_LED_PIN);
      wait(100);
      ledOn(RFID_LED_PIN);
      wait(200);
      ledOff(RFID_LED_PIN);
      wait(100);
      ledOn(RFID_LED_PIN);
      wait(200);
      ledOff(RFID_LED_PIN);
      wait(200);
      rfidDenseMode = false;
    } else {
      Serial.println("RFID: failed to set high sensitivity reader mode.");
    }
  }
}

void rfidSetPowerLevel(int powerLevel) {
  // Set power level based on loaded configuration
  // Lookup table for power level commands (10-26 dBm)
  const unsigned char* powerCommands[] = {
    Power10dbm, Power11dbm, Power12dbm, Power13dbm, Power14dbm,
    Power15dbm, Power16dbm, Power17dbm, Power18dbm, Power19dbm,
    Power20dbm, Power21dbm, Power22dbm, Power23dbm, Power24dbm,
    Power25dbm, Power26dbm
  };
  
  // Clamp power level to valid range (10-26 dBm)
  if (powerLevel < 10) powerLevel = 10;
  if (powerLevel > 26) powerLevel = 26;
  
  // Use lookup table instead of switch statement
  const unsigned char* powerCmd = powerCommands[powerLevel - 10];
  bool ok = setReaderSetting(powerCmd, 9, PowerLevelResponse, 8);

  if(ok) {
    rfidPowerLevel = powerLevel;
    Serial.print("RFID: set power level: ");
    Serial.print(rfidPowerLevel);
    Serial.println(" dBm");
    ledOn(RFID_LED_PIN);
    wait(200);
    ledOff(RFID_LED_PIN);
    wait(200);
  } else {
    Serial.println("RFID: failed to set power level.");
  }
}

void initRfid() {
  Serial.println("RFID: starting...");
  SerialRFID.begin(115200,SERIAL_8N1, RX_PIN, TX_PIN);
  wait(500);

  if(setReaderSetting(StopReadMulti, 7, StopReadMultiResponse, 8)) {
    #ifdef DEBUG
      Serial.println("RFID: Stopped ReadMulti.");
    #endif
  } else {
    Serial.println("RFID: Failed to stop ReadMulti.");
  }

  //set region to Europe
  if(setReaderSetting(Europe, 8, RegionResponse, 8)) {
    Serial.println("RFID: set Europe region.");
    ledOn(RFID_LED_PIN);
    wait(200);
    ledOff(RFID_LED_PIN);
    wait(200);
  } else {
    Serial.println("RFID: failed to set Europe region.");
  }


  //no module sleep time
  if(setReaderSetting(NoModuleSleepTime, 8, NoModuleSleepTimeResponse, 8)) {
    Serial.println("RFID: disabled module sleep time.");
    ledOn(RFID_LED_PIN);
    wait(200);
    ledOff(RFID_LED_PIN);
    wait(200);
  } else {
    Serial.println("RFID: failed to disable module sleep time.");
  }

  // set sensitivity mode
  rfidSetDensitivityMode(rfidDenseMode);
  
  //set power level and start ReadMulti
  rfidSetPowerLevel(rfidPowerLevel);
  Serial.println("RFID: minimum lap time (ms): " + String(minLapTime));
  for (int i=0; i < RFID_MAX_MAPPING_COUNT; i++) {
    if (mappings[i].tagId[0] != '\0') {
      Serial.print("RFID: mapping loaded - tag ID: ");
      Serial.print(mappings[i].tagId);
      Serial.print(" -> mapped ID: ");
      Serial.println(mappings[i].mappedId);
    }
  }
  Serial.println("RFID: ble name: " + bleName);
  Serial.println("RFID: wifi ssid: " + wifiSsid);
  Serial.println("RFID: wifi password: " + wifiPassword);
  Serial.println("RFID: websocket server: " + websocketServer);
  Serial.println("RFID: initialized.");
  SerialRFID.write(ReadMulti,10);
  lastRestart = millis();
}

int getParameterLength() {
  unsigned char paramLengthBytes[2];
  SerialRFID.readBytes(paramLengthBytes, 2);
  parameterLength = paramLengthBytes[0] << 8;
  parameterLength += paramLengthBytes[1];
  dataCheckSum += paramLengthBytes[0] + paramLengthBytes[1];
  #ifdef DEBUG
    Serial.print("Parameter length: ");
    Serial.println(parameterLength);
  #endif
  return parameterLength;
}

void readDataBytes(unsigned char *dataBytes, int dataLength) {
  SerialRFID.readBytes(dataBytes, dataLength);
  #ifdef DEBUG
    Serial.print("Data Bytes:");
  #endif
  for(int i = 0; i < dataLength; i++) {
    dataCheckSum += dataBytes[i];
    #ifdef DEBUG
      Serial.print(" 0x");
      Serial.print(dataBytes[i], HEX);
    #endif
  }
  #ifdef DEBUG
    Serial.println("");
  #endif
  dataCheckSum = (dataCheckSum & 0xFF);
}

bool readRfid() {
  readTag = false;
  parameterLength = 0;
  while(SerialRFID.available() > 0)
  {
    rfidSerialByte = SerialRFID.read();
    if(!startByte && (rfidSerialByte == 0xAA)) {
      startByte = true;
      #ifdef DEBUG
        Serial.println("Got Start Byte");
      #endif
    }
    else if(startByte && !gotMessageType)
    {
      gotMessageType = true;
      messageType = rfidSerialByte;
      #ifdef DEBUG
        Serial.print("Got Message Type: 0x");
        Serial.println(messageType, HEX);
      #endif
      dataCheckSum = rfidSerialByte;
    }
    else if(gotMessageType) {
      command = rfidSerialByte;
      #ifdef DEBUG
        Serial.print("Command: 0x");
        Serial.println(command, HEX);
      #endif
      dataCheckSum += rfidSerialByte;
      if (getParameterLength() > 0) {
        unsigned char dataBytes[parameterLength];
        readDataBytes(dataBytes, parameterLength);
        unsigned char endBytes[2];
        SerialRFID.readBytes(endBytes, 2);
        bool validData = endBytes[0] == dataCheckSum && endBytes[1] == 0xDD;
        if(validData) {
          if(messageType == 0x01) {
            if(command == 0xFF) {
              #ifdef DEBUG
                Serial.println("No label detected.");
              #endif
            }
          }
          else if(messageType == 0x02) {
            if(command == 0x22) {
              processLabelData(dataBytes);
              readTag = true;
            }
          }
          #ifdef DEBUG
            Serial.println("Got valid data frame");
            Serial.println("############################");
          #endif
        }
        else {
          #ifdef DEBUG
            Serial.println("Got invalid data frame");
            Serial.println("############################");
          #endif
        }
      }
      resetRfidData();
    }
    else{
      resetRfidData();
    }
  }
  if(!readTag) {
    now = millis();
    if ((lastRestart + RFID_RESTART_TIME) < now) {
      lastRestart = now;
      #ifdef DEBUG
        Serial.println("Restart ReadMulti");
      #endif
      SerialRFID.write(ReadMulti,10);
      #ifdef DEBUG
        Serial.println("RFID: restarted ReadMulti.");
      #endif
    }
  }
  return readTag;
}

void resetRfidData() {
  startByte = false;
  gotMessageType = false;
  crc = 0;
  rssi = 0;
  pc = 0;
  dataCheckSum = 0;
  command = 0;
  messageType = 0;
}

void processLabelData(unsigned char *dataBytes) {
  //RSSI
  rssi = dataBytes[0];
  #ifdef DEBUG
    Serial.print("RSSI: 0x");
    Serial.println(rssi, HEX);
  #endif
  //PC
  pc = (dataBytes[1] << 8) + dataBytes[2];
  #ifdef DEBUG
    Serial.print("PC: 0x");
    Serial.println(pc, HEX);
  #endif
  //EPC
  for(int i = 3; i < parameterLength-2; i++) {
    epcBytes[i-3] = dataBytes[i];
    #ifdef DEBUG
      if(i == 3) {
        Serial.print("EPC: ");
      }
      Serial.print(epcBytes[i-3], HEX);
    #endif
  }
  crc = (dataBytes[parameterLength-2] << 8) + dataBytes[parameterLength-1];
  #ifdef DEBUG
    Serial.println("");
    Serial.print("CRC: 0x");
    Serial.println(crc, HEX);
  #endif
  checkRfid(epcBytes);
}

void checkRfid(unsigned char epcBytes[]) {
  char buffer[25]; // Genug Platz für 8 Hex-Ziffern + Nullterminator
  // Konvertiere die Byte-Werte in hexadezimale Zeichen und speichere sie in epcBytes
  for (int i = 0; i < 12; i++) {
    sprintf(buffer + (i * 2), "%02X", epcBytes[i]);
  }
  buffer[24] = '\0'; // Nullterminator am Ende hinzufügen
  String epcString(buffer);

  now = millis();
  if (epcString != lastEpcString || (lastEpcRead + minLapTime) < now) {
    sendFinishLineEvent(epcString, now);
    lastEpcString = epcString;
    lastEpcRead = now;
  }
}

bool writeRfidEpc(const int newEpcId) {
  readRfid();
  if (lastEpcString.length() % 2 != 0 && lastEpcString.length() / 2 != 12) {
    return false;
  }
  unsigned char epcBytes[12];
  for (int i = 0; i < 12; i++) {
    char hexPair[3];
    lastEpcString.substring(i * 2, (i * 2) + 2).toCharArray(hexPair, 3);
    // uses sscanf to convert hex pair to unsigned char
    unsigned int byteValue;
    sscanf(hexPair, "%2X", &byteValue);
    epcBytes[i] = static_cast<unsigned char>(byteValue);
  }

  Serial.println("Writing new EPC...");
  //Set Select parameter 
  unsigned char selectCommand[26];
  selectCommand[0] = 0xAA; // Header
  selectCommand[1] = 0x00; // Type
  selectCommand[2] = 0x0C; // Command
  selectCommand[3] = 0x00; // PL(MSB)
  selectCommand[4] = 0x13; // PL(LSB)
  selectCommand[5] = 0x01; // Select Parameter
  selectCommand[6] = 0x00; // Ptr MSB
  selectCommand[7] = 0x00; // Ptr
  selectCommand[8] = 0x00; // Ptr
  selectCommand[9] = 0x20; // Ptr LSB
  selectCommand[10] = 0x60; // Masklength
  selectCommand[11] = 0x00; // Truncation
  selectCommand[12] = epcBytes[0]; // EPC MASK
  selectCommand[13] = epcBytes[1];
  selectCommand[14] = epcBytes[2];
  selectCommand[15] = epcBytes[3];
  selectCommand[16] = epcBytes[4];
  selectCommand[17] = epcBytes[5];
  selectCommand[18] = epcBytes[6];
  selectCommand[19] = epcBytes[7];
  selectCommand[20] = epcBytes[8];
  selectCommand[21] = epcBytes[9];
  selectCommand[22] = epcBytes[10];
  selectCommand[23] = epcBytes[11];
  unsigned char checksum = 0x00;
  for(int i = 2; i < 24; i++) {
      checksum += selectCommand[i];
  }
  selectCommand[24] = checksum; // Checksum
  selectCommand[25] = 0xDD;

  setReaderSetting(StopReadMulti, 7, StopReadMultiResponse, 8);
  unsigned char expectedSelectResponse[8] = {0xAA,0x01,0x0C,0x00,0x01,0x00,0x0E,0xDD};
  while(SerialRFID.available()) {
      SerialRFID.read();
  }
  if(setReaderSetting(selectCommand, 26, expectedSelectResponse, 8)) {
      //write new EPC
      unsigned char writeCommand[28]; // Assuming max 32 words (64 bytes) for DT
      writeCommand[0] = 0xAA; // Header
      writeCommand[1] = 0x00; // Type
      writeCommand[2] = 0x49; // Command
      writeCommand[3] = 0x00; // PL(MSB)
      writeCommand[4] = 0x15; // PL(LSB)
      writeCommand[5] = 0x00; // AP(MSB) - Access Password (assuming no password)
      writeCommand[6] = 0x00;
      writeCommand[7] = 0x00;
      writeCommand[8] = 0x00; // AP(LSB)
      writeCommand[9] = 0x01; // MemBank (EPC)
      writeCommand[10] = 0x00; // SA(MSB) - Start Address (word offset, 0x0002 to skip PC bits)
      writeCommand[11] = 0x02; // SA(LSB)
      writeCommand[12] = 0x00; // DL(MSB) - Data Length (6 words = 12 bytes)
      writeCommand[13] = 0x06; // DL(LSB)
      writeCommand[14] = epcBytes[0]; // DT (New EPC Data)
      writeCommand[15] = epcBytes[1];
      writeCommand[16] = epcBytes[2];
      writeCommand[17] = epcBytes[3];
      writeCommand[18] = epcBytes[4];
      writeCommand[19] = epcBytes[5];
      writeCommand[20] = epcBytes[6];
      writeCommand[21] = epcBytes[7];
      writeCommand[22] = epcBytes[8];
      writeCommand[23] = epcBytes[9];
      writeCommand[24] = epcBytes[10];
      writeCommand[25] = newEpcId & 0xFF; // New EPC ID
      unsigned char checksum = 0x00;
      for(int i = 1; i < 26; i++) {
          checksum += writeCommand[i];
      }
      writeCommand[26] = checksum; // Checksum
      writeCommand[27] = 0xDD; // End
      // Prepare expected response
      unsigned char expectedWriteResponse[23];
      expectedWriteResponse[0] = 0xAA; // Header
      expectedWriteResponse[1] = 0x01; // Type
      expectedWriteResponse[2] = 0x49; // Command
      expectedWriteResponse[3] = 0x00; // PL(MSB)
      expectedWriteResponse[4] = 0x10; // PL(LSB)
      expectedWriteResponse[5] = 0x0E;
      expectedWriteResponse[6] = 0x34; // PC MSB
      expectedWriteResponse[7] = 0x00; // PC LSB
      expectedWriteResponse[8] = epcBytes[0]; // DT (New EPC Data)
      expectedWriteResponse[9] = epcBytes[1];
      expectedWriteResponse[10] = epcBytes[2];
      expectedWriteResponse[11] = epcBytes[3];
      expectedWriteResponse[12] = epcBytes[4];
      expectedWriteResponse[13] = epcBytes[5];
      expectedWriteResponse[14] = epcBytes[6];
      expectedWriteResponse[15] = epcBytes[7];
      expectedWriteResponse[16] = epcBytes[8];
      expectedWriteResponse[17] = epcBytes[9];
      expectedWriteResponse[18] = epcBytes[10];
      expectedWriteResponse[19] = epcBytes[11];
      expectedWriteResponse[20] = 0x00; // Result
      checksum = 0x00;
      for(int i = 1; i < 21; i++) {
          checksum += expectedWriteResponse[i];
      }
      expectedWriteResponse[21] = checksum; // Checksum
      expectedWriteResponse[22] = 0xDD; // End
      while(SerialRFID.available()) {
          SerialRFID.read();
      }
      if(setReaderSetting(writeCommand, 28, expectedWriteResponse, 23)) {
          Serial.println("Wrote EPC to label.");
          ledOn(RFID_LED_PIN);
          ledOn(BLE_LED_PIN);
          wait(200);
          ledOff(RFID_LED_PIN);
          ledOff(BLE_LED_PIN);
          SerialRFID.write(ReadMulti,10);
          return true; // Successfully wrote EPC
      } else {
          Serial.println("Could not write EPC to label.");
      }
  } else {
    Serial.println("Failed to send select command.");
  }
  SerialRFID.write(ReadMulti,10);
  return false; // Failed to write EPC
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

  pinMode(RFID_LED_PIN, OUTPUT);
  ledOn(RFID_LED_PIN);

  pinMode(BLE_LED_PIN, OUTPUT);
  ledOn(BLE_LED_PIN);

  pinMode(WEBSOCKET_LED_PIN, OUTPUT);
  ledOn(WEBSOCKET_LED_PIN);
  wait(1000);
  ledOff(RFID_LED_PIN);
  ledOff(BLE_LED_PIN);
  ledOff(WEBSOCKET_LED_PIN);

  #ifdef PUSH_BUTTON_PIN
    pinMode(PUSH_BUTTON_PIN, INPUT_PULLUP);
  #endif

  //init rfid storage
  resetRfidStorage();

  Serial.begin(19200);
  wait(2000);

  Serial.print("RFID-Connector Version: ");
  Serial.println(VERSION);
  Serial.println("############################");
  printCmdList();

  configurationLoad();

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
        while (res == false and retry < 5) {
          connectWebsocket();
          retry += 1;
        }
      }
    } else {
      Serial.println("\nFailed to connect to WiFi.");
    }
  }

  // Initialize BLE
  initBLE();

  // Start RFID reader
  initRfid();
}

void loop() {
  processSerialCommands();
  readTag = readRfid();
  now = millis();

  if(!readTag) {
    if((RfidLedOnMs > 0) && (RfidLedOnMs + RFID_LED_ON_TIME) < now) {
      ledOff(RFID_LED_PIN);
    }
    #ifdef PUSH_BUTTON_PIN
      if(digitalRead(PUSH_BUTTON_PIN) == LOW) {
        if(!buttonWasPressed  && now - lastButtonChange > debounceDelay) {
          buttonWasPressed = true;
          resetRfidStorage();
          ledOn(RFID_LED_PIN);
        }
      } else {
        if(buttonWasPressed && now - lastButtonChange > debounceDelay) {
          buttonWasPressed = false;
          lastButtonChange = now;
          ledOff(RFID_LED_PIN);
        }
      }
    #endif
  }
  if (websocketWasConnected) {
    if(websocketClient != nullptr && websocketClient->available()) {
      websocketClient->poll();
      if(now > (websocketLastPing + WEBSOCKET_PING_INTERVAL)) {
        websocketLastPing = now;
        websocketClient->ping();
      }
    }
    else if(WiFi.status() == WL_CONNECTED) {
      ledOff(WEBSOCKET_LED_PIN);
      bool res = connectWebsocket();
      int retry = 0;
      while (res == false and retry < 5) {
        connectWebsocket();
        retry += 1;
      }
    }
    else {
      delete websocketClient;
      websocketClient = nullptr;
      ledOff(WEBSOCKET_LED_PIN);
    }
  }
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
  setReaderSetting(StopReadMulti, 7, StopReadMultiResponse, 8);
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
  websocketClient = new WebsocketsClient(); // Überschreibt das alte Objekt

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

void onMessageCallback(WebsocketsMessage message) {
  Serial.print("Websocket: got message: ");
  Serial.println(message.data());
}

void onEventsCallback(WebsocketsEvent event, String data) {
  if(event == WebsocketsEvent::ConnectionOpened) {
      Serial.println("Websocket: connected");
      ledOn(WEBSOCKET_LED_PIN);
      websocketWasConnected = true;
  } else if(event == WebsocketsEvent::ConnectionClosed) {
    Serial.println("Websocket: connection closed");
    delete websocketClient;
    websocketClient = nullptr;
    ledOff(WEBSOCKET_LED_PIN);
  } else if(event == WebsocketsEvent::GotPing) {
    websocketClient->pong();
  } else if(event == WebsocketsEvent::GotPong) {
    #ifdef DEBUG
      Serial.println("Websocket: got a pong!");
    #endif
  }
}
    
