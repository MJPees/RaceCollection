#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <Preferences.h>
#include <ArduinoWebsockets.h> //ArduinoWebsockets 0.5.4
#include <ArduinoJson.h>

/* configuration */
#define RFID_DEFAULT_POWER_LEVEL 26 // default power level
#define DEFAULT_MIN_LAP_TIME 3000 //min time between laps in ms
#define WIFI_AP_SSID "RFID-Connector-Config"
#define WIFI_DEFAULT_HOSTNAME "rfid-connector"
#define PREFERENCES_NAMESPACE "racebox"

#define VERSION "1.5.0" // dont forget to update the releases.json
//#define DEBUG

//#define ESP32C3
#define ESP32DEV

//#define INVERT_LEDS
//#define RFID_Connector_PCB_V1_0

#ifdef ESP32DEV
  #define SerialRFID Serial2
  #ifdef RFID_Connector_PCB_V1_0
    #define RX_PIN 17
    #define TX_PIN 16
    #define RFID_LED_PIN 32
    #define WEBSOCKET_LED_PIN 33
    #define WIFI_AP_LED_PIN 25
    #define PUSH_BUTTON_PIN 23
  #else
    #define RX_PIN 16
    #define TX_PIN 17
    #ifdef INVERT_LEDS
      #define RFID_LED_PIN 32
      #define WEBSOCKET_LED_PIN 33
      #define WIFI_AP_LED_PIN 25
      #define PUSH_BUTTON_PIN 23
    #else
      #define RFID_LED_PIN 2
      #define WEBSOCKET_LED_PIN 4
      #define WIFI_AP_LED_PIN 25
      #define PUSH_BUTTON_PIN 23
    #endif
  #endif
#elif defined(ESP32C3)
  HardwareSerial SerialRFID(1);
  #define RX_PIN 5
  #define TX_PIN 6
  #define RFID_LED_PIN 8
  #define WEBSOCKET_LED_PIN 9
  #define WIFI_AP_LED_PIN 10
#endif

#define WIFI_CONNECT_ATTEMPTS 10
#define WIFI_CONNECT_DELAY_MS 1000
#define WEBSOCKET_PING_INTERVAL 5000
#define RFID_LED_ON_TIME 200

#define RFID_REPEAT_TIME 3000
#define RFID_RESTART_TIME 300000
#define RFID_MAX_COUNT 30 // max für freies Fahren laut Carrera
#define RFID_SMARTRACE_MAX_COUNT 8
#define RFID_STORAGE_COUNT 2

#define debounceDelay 50

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

unsigned char epcBytes[12] = {0,0,0,0,0,0,0,0,0,0,0,0};
String lastEpcString = "";
unsigned long lastEpcRead = 0;
unsigned long lastRestart = 0;
unsigned long RfidLedOnMs = 0;
unsigned long lastButtonChange = 0;
unsigned long now = 0;
bool readTag = false;
bool buttonWasPressed = false;

int minLapTime = DEFAULT_MIN_LAP_TIME;

bool wifiApMode = false;
bool webserverRunning = false;

using namespace websockets;
String websocketServer = "";
String websocketCaCert = "";
bool websocketConnected = false;
unsigned long websocketLastPing = 0;
unsigned long websocketLastAttempt = 0;
unsigned long websocketBackoff = 1000; // Start mit 1 Sekunde
const unsigned long websocketMaxBackoff = 30000; // Maximal 30 Sekunden
WebsocketsClient client;

WebServer server(80);
DNSServer dnsServer;

Preferences preferences;

String targetSystem;
String wifiSsid;
String wifiPassword;
String wifiHostname;

String smartRaceWebsocketServer;

String chRacingClubWebsocketServer;
String chRacingClubCaCert;
String chRacingClubApiKey;

int rfidPowerLevel = RFID_DEFAULT_POWER_LEVEL;
bool rfidDenseMode = true;
int newEpcId = 1;

struct rfid_data {
  String id[RFID_STORAGE_COUNT];
  String name;
  unsigned long last;
};
rfid_data rfids[RFID_MAX_COUNT];

unsigned long lastResetTime = 0;


void configurationSave() {
  preferences.putString("target_system", targetSystem);
  preferences.putInt("power_level", rfidPowerLevel);
  preferences.putBool("rfid_dense_mode", rfidDenseMode);
  preferences.putInt("min_lap_time", minLapTime);

  preferences.putString("wifi_ssid", wifiSsid);
  preferences.putString("wifi_password", wifiPassword);
  preferences.putString("wifi_hostname", wifiHostname);

  preferences.putString("sr_ws_server", smartRaceWebsocketServer);

  preferences.putString("chrc_ws_server", chRacingClubWebsocketServer);
  preferences.putString("chrc_ws_ca_cert", chRacingClubCaCert);
  preferences.putString("chrc_api_key", chRacingClubApiKey);

  if (targetSystem == "smart_race") {
    char key[20];
    for(int i=0; i<RFID_SMARTRACE_MAX_COUNT; i++) {
      snprintf(key, sizeof(key), "RFID%d", i);
      preferences.putString(key, rfids[i].name);
      for(int j=0; j<RFID_STORAGE_COUNT; j++) {
        snprintf(key, sizeof(key), "RFID%d_%d", i, j);
        preferences.putString(key, rfids[i].id[j]);
      }
    }
  }
}

void loadRfidStorage() {
  char key[20];
  char defaultName[20];
  for(int i=0; i<RFID_SMARTRACE_MAX_COUNT; i++) {
    snprintf(key, sizeof(key), "RFID%d", i);
    snprintf(defaultName, sizeof(defaultName), "Controller %d", i+1);
    rfids[i].name = preferences.getString(key, defaultName);
    for(int j=0; j<RFID_STORAGE_COUNT; j++) {
      snprintf(key, sizeof(key), "RFID%d_%d", i, j);
      rfids[i].id[j] = preferences.getString(key, "");
    }
  }
}

void configurationLoad() {
  targetSystem = preferences.getString("target_system", "smart_race");
  rfidDenseMode = preferences.getBool("rfid_dense_mode", true);
  rfidPowerLevel = preferences.getInt("power_level", RFID_DEFAULT_POWER_LEVEL);
  minLapTime = preferences.getInt("min_lap_time", DEFAULT_MIN_LAP_TIME);

  wifiSsid = preferences.getString("wifi_ssid", "");
  wifiPassword = preferences.getString("wifi_password", "");
  wifiHostname = preferences.getString("wifi_hostname", WIFI_DEFAULT_HOSTNAME);

  smartRaceWebsocketServer = preferences.getString("sr_ws_server", "");

  chRacingClubWebsocketServer = preferences.getString("chrc_ws_server", "");
  chRacingClubCaCert = preferences.getString("chrc_ws_ca_cert", "");
  chRacingClubApiKey = preferences.getString("chrc_api_key", "");

  if (targetSystem == "smart_race") {
    websocketServer = smartRaceWebsocketServer;
    websocketCaCert = "";
    loadRfidStorage();

    Serial.println("\nConfiguration: RFID-Connector loaded");
  }

  if (targetSystem == "ch_racing_club") {
    websocketServer = chRacingClubWebsocketServer;
    websocketCaCert = chRacingClubCaCert;
    resetRfidStorage();

    Serial.println("\nConfiguration: CH Racing Club loaded");
  }
}

void handleNotFound() {
  server.sendHeader("Location", "/");
  server.send(302, "text/plain", "redirect to configuration page");
}

void handleRoot() {
  String html = "<!DOCTYPE html><html><head><title>RFID-Connector</title>";
  html += "<meta charset='UTF-8'>"; // Specify UTF-8 encoding
  html += "<style>";
  html += "*, *::before, *::after {box-sizing: border-box;}";
  html += "body {min-height: 100vh;margin: 0;}";
  html += "form {max-width: 535px;margin: 0 auto;}";
  html += "label {margin-bottom: 5px;display:block;}";
  html += "input[type=text],input[type=password],input[type=number],select,textarea {width: 100%;padding: 8px;border: 1px solid #ccc;border-radius: 4px;display: block;}";
  html += "input[type=submit] {width: 100%;background-color: #4CAF50;color: white;padding: 10px 15px;border: none;border-radius: 4px;}";
  html += ".checkbox-container { display: flex; align-items: center; gap: 5px; }";
  html += "</style>";
  html += "<script src='https://unpkg.com/alpinejs' defer></script>";
  html += "</head><body>";
  html += "<form x-data=\"{ targetSystem: '" + targetSystem + "', smartRaceWebsocketServer: '" + smartRaceWebsocketServer + "', racingClubWebsocketServer: '" + chRacingClubWebsocketServer + "' }\" action='/config' method='POST'>";
  html += "<h1 align=center>RFID-Connector</h1>";
  html += "<div style=\"text-align: center; margin-bottom:20px;\">";
  html += "<a href=\"http://" + wifiHostname + "/label-writer\">Label Writer</a><br>";
  html += "</div>";

  html += "<label for='wifi_ssid'>SSID:</label>";
  html += "<input type='text' id='wifi_ssid' name='wifi_ssid' value='" + wifiSsid + "'><br>";

  html += "<label for='wifi_password'>Passwort:</label>";
  html += "<input type='password' id='wifi_password' name='wifi_password' value='" + wifiPassword + "'><br>";

  html += "<label for='wifi_hostname'>Hostname:</label>";
  html += "<input type='text' id='wifi_hostname' name='wifi_hostname' value='" + wifiHostname + "'><br>";

  if (!wifiApMode) {
    html += "<label for='target_system'>Target System:</label>";
    html += "<select id='target_system' name='target_system' x-model='targetSystem'>";
    html += "<option value='smart_race'" + String((targetSystem == "smart_race") ? " selected" : "") + ">SmartRace</option>";
    html += "<option value='ch_racing_club'" + String((targetSystem == "ch_racing_club") ? " selected" : "") + ">CH Racing Club</option>";
    html += "</select><br>";

    html += "<div x-show=\"targetSystem == 'ch_racing_club'\">";
    html += "  <label for='ch_racing_club_websocket_server'>Websocket Server CH-Racing-Club:</label>";
    html += "  <input type='text' id='ch_racing_club_websocket_server' name='ch_racing_club_websocket_server' placeholder='ws:// or wss://' x-model='racingClubWebsocketServer' value='" + chRacingClubWebsocketServer + "'><br>";
    html += "  <div x-show=\"racingClubWebsocketServer.startsWith('wss')\">";
    html += "    <label for='ch_racing_club_websocket_ca_cert'>Websocket SSL CA Certificate (PEM) CH-Racing-Club:</label>";
    html += "    <textarea id='ch_racing_club_websocket_ca_cert' name='ch_racing_club_websocket_ca_cert' rows='12' cols='64' style='font-family:monospace;width:100%;'>" + chRacingClubCaCert + "</textarea><br>";
    html += "  </div>";
    html += "  <label for='ch_racing_club_api_key'>ApiKey CH-Racing-Club:</label>";
    html += "  <input type='text' id='ch_racing_club_api_key' name='ch_racing_club_api_key' value='" + chRacingClubApiKey + "'><br>";
    html += "</div>";

    html += "<div x-show=\"targetSystem == 'smart_race'\">";
    html += "  <label for='smart_race_websocket_server'>Websocket Server SmartRace:</label>";
    html += "  <input type='text' id='smart_race_websocket_server' name='smart_race_websocket_server' placeholder='ws://' x-model='smartRaceWebsocketServer' value='" + smartRaceWebsocketServer + "'><br>";
    html += "</div>";

    html += "<label for='min_lap_time'>Minimum Lap Time (ms):</label>";
    html += "<input type='number' id='min_lap_time' name='min_lap_time' value='" + String(minLapTime) + "'><br>";

    html += "<label for='rfid_power_level'>Power Level:</label>";
    html += "<select id='rfid_power_level' name='rfid_power_level'>";
    html += "<option value='10'" + String((rfidPowerLevel == 10) ? " selected" : "") + ">10 dBm</option>";
    html += "<option value='11'" + String((rfidPowerLevel == 11) ? " selected" : "") + ">11 dBm</option>";
    html += "<option value='12'" + String((rfidPowerLevel == 12) ? " selected" : "") + ">12 dBm</option>";
    html += "<option value='13'" + String((rfidPowerLevel == 13) ? " selected" : "") + ">13 dBm</option>";
    html += "<option value='14'" + String((rfidPowerLevel == 14) ? " selected" : "") + ">14 dBm</option>";
    html += "<option value='15'" + String((rfidPowerLevel == 15) ? " selected" : "") + ">15 dBm</option>";
    html += "<option value='16'" + String((rfidPowerLevel == 16) ? " selected" : "") + ">16 dBm</option>";
    html += "<option value='17'" + String((rfidPowerLevel == 17) ? " selected" : "") + ">17 dBm</option>";
    html += "<option value='18'" + String((rfidPowerLevel == 18) ? " selected" : "") + ">18 dBm</option>";
    html += "<option value='19'" + String((rfidPowerLevel == 19) ? " selected" : "") + ">19 dBm</option>";
    html += "<option value='20'" + String((rfidPowerLevel == 20) ? " selected" : "") + ">20 dBm</option>";
    html += "<option value='21'" + String((rfidPowerLevel == 21) ? " selected" : "") + ">21 dBm</option>";
    html += "<option value='22'" + String((rfidPowerLevel == 22) ? " selected" : "") + ">22 dBm</option>";
    html += "<option value='23'" + String((rfidPowerLevel == 23) ? " selected" : "") + ">23 dBm</option>";
    html += "<option value='24'" + String((rfidPowerLevel == 24) ? " selected" : "") + ">24 dBm</option>";
    html += "<option value='25'" + String((rfidPowerLevel == 25) ? " selected" : "") + ">25 dBm</option>";
    html += "<option value='26'" + String((rfidPowerLevel == 26) ? " selected" : "") + ">26 dBm</option>";
    html += "</select><br>";
    html += "<div class='checkbox-container'>";
    html += "<label for='dense_mode'>Use RFID-Dense-Mode:</label>";
    if (rfidDenseMode) {
      html += "<input type='checkbox' id='dense_mode' name='dense_mode' checked>";
    } else {
      html += "<input type='checkbox' id='dense_mode' name='dense_mode'>";
    }
    html += "</div><br>";
  }

  html += "<input type='submit' style='margin-bottom:20px;' value='Speichern'>";
  if (!wifiApMode && targetSystem == "smart_race") {
    html += "<div style='display: grid; grid-template-columns: 1fr; gap: 5px;'>"; // Äußerer Grid-Container
    for (int i = 0; i < RFID_SMARTRACE_MAX_COUNT; i++) {
        html += "<div style='border: 3px solid black; padding: 10px; margin: 5px; display: grid; grid-template-columns: 1fr; gap: 5px;'>"; // Rahmen mit Grid-Spalte
        html += "<div style='display: grid; grid-template-columns: auto 1fr; align-items: center;'>"; // Grid für Name
        html += "<label for='name" + String(i) + "'>Name:</label>";
        html += "<input type='text' maxlength='100' style='width:auto; margin-left: 4px;' id='name" + String(i) + "' name='name" + String(i) + "' value='" + rfids[i].name + "'>";
        html += "</div>";
        html += "<div style='display: grid; grid-template-columns: repeat(" + String(RFID_STORAGE_COUNT * 2) + ", auto); align-items: center;'>"; // Grid für horizontale IDs
        for(int j = 0; j < RFID_STORAGE_COUNT; j++){
          if(j>0) {
            html += "<label style='margin-left: 4px;' for='id" + String(i) + "_" + String(j) + "'>ID" + String(j+1) + ":</label>";
          }
          else {
            html += "<label for='id" + String(i) + "_" + String(j) + "'>ID" + String(j+1) + ":</label>";
          }
          html += "<input type='text' style='width:auto; margin-left: 4px;' id='id" + String(i) + "_" + String(j) + "' name='id" + String(i) + "_" + String(j) + "' value='" + rfids[i].id[j] + "'>";
        }
        html += "</div>"; // Ende Container für horizontale IDs
        html += "</div>";
    }
    html += "</div>"; // Ende äußerer Grid-Container
  }
  html += "</form>";
  html += "</body></html>";
  server.send(200, "text/html", html);
}

void handleLabelWriter() {
  String html = "<!DOCTYPE html><html><head><title>Label-Writer</title>";
  html += "<meta charset='UTF-8'>"; // Specify UTF-8 encoding
  html += "<style>";
  html += "*, *::before, *::after {box-sizing: border-box;}";
  html += "body {min-height: 100vh;margin: 0;}";
  html += "form {max-width: 250px;margin: 0 auto;}";
  html += "label {margin-bottom: 5px;display:block;}";
  html += "input[type=text],input[type=password],input[type=number],select,textarea {width: 100%;padding: 8px;border: 1px solid #ccc;border-radius: 4px;display: block;}";
  html += "input[type=submit] {width: 100%;background-color: #4CAF50;color: white;padding: 10px 15px;border: none;border-radius: 4px;}";
  html += ".checkbox-container { display: flex; align-items: center; gap: 5px; }";
  html += "</style>";
  html += "</head><body>";
  html += "<form action='/write-epc' method='POST'>";
  html += "<h1 align=center>Label-Writer</h1>";
  html += "<div style=\"text-align: center; margin-bottom:20px;\">";
  html += "<a href=\"http://" + wifiHostname + "\">RFID-Connector</a><br>";
  html += "</div>";
  html += "<label for='new_epc_id'>New EPC:</label>";
  html += "<input type='number' id='new_epc_id' name='new_epc_id' value=" + String(newEpcId) +" min='0' max='255'><br>";
  html += "<input type='submit' style='margin-bottom:20px;' value='write EPC'>";
  html += "</form>";
  html += "</body></html>";
  server.send(200, "text/html", html);
}

void handleWriteEpc() {
  if (server.hasArg("new_epc_id")) {
    int newEpcId = server.arg("new_epc_id").toInt();
    if (newEpcId >= 0 && newEpcId <= 255) {
      if(writeRfidEpc(newEpcId)) {
        server.send(200, "text/html", "<!DOCTYPE html><html><head><title>Label-Writer</title></head><body><h1 align=center>EPC ID updated successfully.</h1><p>You will be redirected in 2 seconds.</p><script>setTimeout(function() { window.location.href = 'http://" + wifiHostname + "/label-writer'; }, 2000);</script></body></html>");
      }
      else {
        server.send(200, "text/html", "<!DOCTYPE html><html><head><title>Label-Writer</title></head><body><h1 align=center>Could not write EPC ID.</h1><p>You will be redirected in 2 seconds.</p><script>setTimeout(function() { window.location.href = 'http://" + wifiHostname + "/label-writer'; }, 2000);</script></body></html>");
      }
    } else {
      server.send(200, "text/html", "<!DOCTYPE html><html><head><title>Label-Writer</title></head><body><h1 align=center>Invalid EPC ID. Must be between 0 and 255.</h1><p>You will be redirected in 2 seconds.</p><script>setTimeout(function() { window.location.href = 'http://" + wifiHostname + "/label-writer'; }, 2000);</script></body></html>");
    }
  } else {
    server.send(200, "text/html", "<!DOCTYPE html><html><head><title>Label-Writer</title></head><body><h1 align=center>No EPC ID provided.</h1><p>You will be redirected in 2 seconds.</p><script>setTimeout(function() { window.location.href = 'http://" + wifiHostname + "/label-writer'; }, 2000);</script></body></html>");
  }
}

void handleConfig() {
  if (server.args() > 0) {
    if(setReaderSetting(StopReadMulti, 7, StopReadMultiResponse, 8)) {
      Serial.println("RFID: stopped ReadMulti.");
    } else {
      Serial.println("RFID: failed to stop ReadMulti.");
    }
    bool reConnectWifi = wifiSsid != server.arg("wifi_ssid") || wifiPassword != server.arg("wifi_password") || wifiHostname != server.arg("wifi_hostname");
    bool reConnectWebsocket = false;

    wifiSsid = server.arg("wifi_ssid");
    wifiPassword = server.arg("wifi_password");
    wifiHostname = server.arg("wifi_hostname");

    if(!wifiApMode) {
      reConnectWebsocket = targetSystem != server.arg("target_system");

      if (server.hasArg("dense_mode")) {
        rfidDenseMode = true;
      } else {
          rfidDenseMode = false;
      }
      targetSystem = server.arg("target_system");
      rfidPowerLevel = server.arg("rfid_power_level").toInt();
      minLapTime = server.arg("min_lap_time").toInt();
      if (targetSystem == "smart_race") {
        if (smartRaceWebsocketServer != server.arg("smart_race_websocket_server")) {
          reConnectWebsocket = true; // Reconnect if the server has changed
          smartRaceWebsocketServer = server.arg("smart_race_websocket_server");
        }
        websocketServer = smartRaceWebsocketServer;
        websocketCaCert = ""; // SmartRace does not use a CA cert
        if (reConnectWebsocket == true) {
          loadRfidStorage();
        }
        else {
          char nameKey[12];
          char idKey[16];
          for (int i = 0; i < RFID_SMARTRACE_MAX_COUNT; i++) {
            snprintf(nameKey, sizeof(nameKey), "name%d", i);
            rfids[i].name = server.arg(nameKey);
            for (int j = 0; j < RFID_STORAGE_COUNT; j++) {
              snprintf(idKey, sizeof(idKey), "id%d_%d", i, j);
              rfids[i].id[j] = server.arg(idKey);
            }
          }
        }
      }
      else if (targetSystem == "ch_racing_club") {
        if (chRacingClubWebsocketServer != server.arg("ch_racing_club_websocket_server")) {
          reConnectWebsocket = true; // Reconnect if the server has changed
          chRacingClubWebsocketServer = server.arg("ch_racing_club_websocket_server");
        }
        String newChRacingClubCaCert = server.arg("ch_racing_club_websocket_ca_cert");
        newChRacingClubCaCert.replace("\r", "");
        if( chRacingClubCaCert != newChRacingClubCaCert) {
          reConnectWebsocket = true; // Reconnect if the CA cert has changed
          chRacingClubCaCert = newChRacingClubCaCert;
        }
        if(chRacingClubApiKey != server.arg("ch_racing_club_api_key")) {
          reConnectWebsocket = true; // Reconnect if the API key has changed
          chRacingClubApiKey = server.arg("ch_racing_club_api_key");
        }
        websocketServer = chRacingClubWebsocketServer;
        websocketCaCert = chRacingClubCaCert;
        resetRfidStorage();
      }
    }

    configurationSave();

    server.send(200, "text/html", "<!DOCTYPE html><html><head><title>RFID-Connector</title></head><body><h1>Configuration saved!</h1><p>You will be redirected in 2 seconds.</p><script>setTimeout(function() { window.location.href = 'http://" + wifiHostname + "'; }, 2000);</script></body></html>");
    wait(500);
    if (reConnectWebsocket) {
      if(websocketConnected) {
        client.close();
        wait(500);
      }
      websocketLastAttempt = 0;
      connectWebsocket();
    }

    if(reConnectWifi) {
      wifiReload();
    }
    rfidSetDensitivityMode(rfidDenseMode);
    rfidSetPowerLevel(rfidPowerLevel);
    SerialRFID.write(ReadMulti,10);
    Serial.println("RFID: started ReadMulti.");
  } else {
    server.send(200, "text/html", "<!DOCTYPE html><html><head><title>RFID-Connector</title></head><body><h1>Invalid request!</h1><p>You will be redirected in 2 seconds.</p><script>setTimeout(function() { window.location.href = 'http://" + wifiHostname + "'; }, 2000);</script></body></html>");
  }
}

void wifiReload() {
  Serial.println("WiFi: Initiating reload...");
  if (WiFi.getMode() != WIFI_OFF) {
    Serial.println("WiFi: Disconnecting existing connections...");
    if (WiFi.status() == WL_CONNECTED) {
      WiFi.disconnect(true);
      wait(500);
    }
    WiFi.softAPdisconnect(true);
    wait(500);
  }
  if(dnsServer.isUp()) {
    dnsServer.stop();
    wait(100);
  }

  WiFi.setHostname(wifiHostname.c_str());
  Serial.print("WiFi: Attempting to connect to AP '");
  Serial.print(wifiSsid);
  Serial.println("'...");
  WiFi.mode(WIFI_STA);
  WiFi.begin(wifiSsid.c_str(), wifiPassword.c_str());

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < WIFI_CONNECT_ATTEMPTS) {
    waitForWifi(WIFI_CONNECT_DELAY_MS);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("\nWiFi: connected ");
    Serial.println(WiFi.localIP());
    Serial.print("WiFi: Hostname: ");
    Serial.println(WiFi.getHostname());
    wifiApMode = false;
    ledOff(WIFI_AP_LED_PIN);
  } else {
    Serial.println("\nWiFi: connect failed, starting AP mode");
    WiFi.disconnect(true);
    wait(500);
    WiFi.softAP(WIFI_AP_SSID);
    dnsServer.start();
    wifiApMode = true;
    ledOn(WIFI_AP_LED_PIN);
    Serial.print("\nWiFi: AP started, IP: ");
    Serial.println(WiFi.softAPIP());
  }
}

void connectWebsocket() {
  now = millis();
  if (now - websocketLastAttempt < websocketBackoff) {
    return; // wait until backoff time is reached
  }
  websocketLastAttempt = now;
  setReaderSetting(StopReadMulti, 7, StopReadMultiResponse, 8);

  client = WebsocketsClient(); // Überschreibt das alte Objekt

  client.onMessage(onMessageCallback);
  client.onEvent(onEventsCallback);

  Serial.print("Websocket: connecting ");
  Serial.println(websocketServer);

  if (websocketCaCert.length() > 0) {
    client.setCACert(websocketCaCert.c_str());
  }
  websocketConnected = client.connect(websocketServer);

  if(websocketConnected) {
    JsonDocument doc;
    if(targetSystem == "smart_race") {
      doc["type"] = "controller_set";
      doc["data"]["controller_id"] = "Z";
    }

    if(targetSystem == "ch_racing_club") {
      doc["command"] = "rfid_connect";
      doc["data"]["name"] = wifiHostname.c_str();
      doc["data"]["ip"] = WiFi.localIP().toString();
      doc["data"]["api_key"] = chRacingClubApiKey;
      doc["data"]["version"] = VERSION;
    }

    char output[256];
    serializeJson(doc, output);
    client.ping();
    client.send(output);
    websocketBackoff = 1000; // reset backoff time on successful connection
  } else {
    Serial.println("Websocket: connection failed.");
    if (targetSystem == "ch_racing_club") {
      websocketBackoff = min(websocketBackoff * 2, websocketMaxBackoff); // Exponentielles Backoff
    }
    else {
      websocketBackoff = 3000; // set backoff time for SmartRace
    }
  }
  SerialRFID.write(ReadMulti,10);
}

void sendFinishLineMessage(int controller_id, unsigned long timestamp, String rfidString) {
  int last_timestamp = rfids[controller_id].last;
  int lap_time = last_timestamp > 0 ? timestamp - last_timestamp : timestamp;

  // set last timestamp for the controller
  rfids[controller_id].last = timestamp;

  // build the message
  JsonDocument doc;
  if (targetSystem == "ch_racing_club") {
    doc["command"] = "lap";
    doc["data"]["api_key"] = chRacingClubApiKey;
    doc["data"]["rfid"] = rfidString;
    doc["data"]["duration"] = lap_time;
  } else {
    doc["type"] = "analog_lap";
    doc["data"]["timestamp"] = rfids[controller_id].last;
    doc["data"]["controller_id"] = controller_id + 1;
  }

  // send the message via websocket
  char output[256];
  serializeJson(doc, output);
  if(websocketConnected) {
    ledOn(RFID_LED_PIN);
    client.send(output);
  } else {
    #ifndef DEBUG
      Serial.print("Websocket is not connected!: ");
      Serial.println(output);
    #endif
  }

  // print the message to the serial console
  #ifdef DEBUG
    Serial.print("Websocket: ");
    Serial.println(output);
  #endif
}

void sendFinishLineEvent(String rfidString, unsigned long ms) {
  bool found = false;
  int maxRfidCnt = RFID_MAX_COUNT;
  if (targetSystem == "smart_race") {
    maxRfidCnt = RFID_SMARTRACE_MAX_COUNT;
  }
  for(int j = 0; j<RFID_STORAGE_COUNT;j++) {
    for (int i = 0; i < maxRfidCnt; i++) {
      if(rfids[i].id[j] == rfidString) {
        if(rfids[i].last + minLapTime < ms) {
          sendFinishLineMessage(i, ms, rfidString);
        }
        found = true;
        break;
      }
    }
    if (found) {
      break;
    }
  }
  if(!found) {
    for(int i=0; i < maxRfidCnt; i++) {
      if(rfids[i].id[0] == "") {
        rfids[i].id[0] = rfidString;
        Serial.print("RFID: new car at controller id: ");
        Serial.print(i+1);
        Serial.print(rfids[i].id[0]);
        Serial.println();
        sendFinishLineMessage(i, ms, rfidString);
        break;
      }
    }
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
  // TODO on race aborted or finished => resetRfidStorage();

  #ifdef DEBUG
    Serial.print("Websocket: received message: ");
    serializeJsonPretty(doc, Serial);
    Serial.println();
  #endif
  if (targetSystem == "ch_racing_club") {
    //serializeJsonPretty(doc, Serial);
    //Serial.println();
    const char* message = doc["message"] | ""; // Falls "message" nicht existiert, wird ein leerer String zurückgegeben

    if (message[0] != '\0') {
      //Serial.print(F("Primäre Message: "));
      //Serial.println(message);
      // Prüfen, ob die Nachricht ein verschachteltes JSON (Command) ist
      if (message[0] == '{') {
          //Serial.println(F("Dies ist ein Command-JSON."));
          JsonDocument commandDoc;
          deserializeJson(commandDoc, message);
          const char* command = commandDoc["command"] | "";
          //Serial.println(command);
          if (strcmp(command, "reset rfid") == 0) {
            resetRfidStorage();
            Serial.println("reset of rfid storage");
          }
      }
    } else {
      //Serial.println(F("Feld 'message' nicht gefunden."));
    }
  }
}

void onEventsCallback(WebsocketsEvent event, String data) {
  if(event == WebsocketsEvent::ConnectionOpened) {
      Serial.println("Websocket: connected");
      websocketConnected = true;
      ledOn(WEBSOCKET_LED_PIN);
  } else if(event == WebsocketsEvent::ConnectionClosed) {
    Serial.println("Websocket: connection closed");
    websocketConnected = false;
    ledOff(WEBSOCKET_LED_PIN);
  } else if(event == WebsocketsEvent::GotPing) {
    #ifdef DEBUG
      Serial.println("Websocket: got a ping!");
    #endif
    client.pong();
  } else if(event == WebsocketsEvent::GotPong) {
    #ifdef DEBUG
      Serial.println("Websocket: got a pong!");
    #endif
  }
}

void wait(unsigned long waitTime) {
  unsigned long startWaitTime = millis();
  while((millis() - startWaitTime) < waitTime) {
    if (webserverRunning) server.handleClient();
    else delay(1);
    client.poll();
  }
}

void waitForWifi(unsigned long waitTime) {
  unsigned long startWaitTime = millis();
  while((millis() - startWaitTime) < waitTime) {
    if(WiFi.status() == WL_CONNECTED) return; // exit if WiFi is connected
    delay(10);
  }
}

void resetRfidStorage() {
  char nameBuf[20];
  now = millis();
  for(int i=0; i < RFID_MAX_COUNT; i++) {
    for(int j=0; j<RFID_STORAGE_COUNT; j++) {
      rfids[i].id[j] = "";
    }
    snprintf(nameBuf, sizeof(nameBuf), "Controller %d", i+1);
    rfids[i].name = nameBuf;
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
    } else {
      Serial.println("RFID: failed to set high sensitivity reader mode.");
    }
  }
}

void rfidSetPowerLevel(int rfidPowerLevel) {
  // Set power level based on loaded configuration
  bool ok;

  switch (rfidPowerLevel) {
    case 10:
      ok = setReaderSetting(Power10dbm, 9, PowerLevelResponse, 8);
      break;
    case 11:
      ok = setReaderSetting(Power11dbm, 9, PowerLevelResponse, 8);
      break;
    case 12:
      ok = setReaderSetting(Power12dbm, 9, PowerLevelResponse, 8);
      break;
    case 13:
      ok = setReaderSetting(Power13dbm, 9, PowerLevelResponse, 8);
      break;
    case 14:
      ok = setReaderSetting(Power14dbm, 9, PowerLevelResponse, 8);
      break;
    case 15:
      ok = setReaderSetting(Power15dbm, 9, PowerLevelResponse, 8);
      break;
    case 16:
      ok = setReaderSetting(Power16dbm, 9, PowerLevelResponse, 8);
      break;
    case 17:
      ok = setReaderSetting(Power17dbm, 9, PowerLevelResponse, 8);
      break;
    case 18:
      ok = setReaderSetting(Power18dbm, 9, PowerLevelResponse, 8);
      break;
    case 19:
      ok = setReaderSetting(Power19dbm, 9, PowerLevelResponse, 8);
      break;
    case 20:
      ok = setReaderSetting(Power20dbm, 9, PowerLevelResponse, 8);
      break;
    case 21:
      ok = setReaderSetting(Power21dbm, 9, PowerLevelResponse, 8);
      break;
    case 22:
      ok = setReaderSetting(Power22dbm, 9, PowerLevelResponse, 8);
      break;
    case 23:
      ok = setReaderSetting(Power23dbm, 9, PowerLevelResponse, 8);
      break;
    case 24:
      ok = setReaderSetting(Power24dbm, 9, PowerLevelResponse, 8);
      break;
    case 25:
      ok = setReaderSetting(Power25dbm, 9, PowerLevelResponse, 8);
      break;
    default:
      ok = setReaderSetting(Power26dbm, 9, PowerLevelResponse, 8);
      break;
  }

  if(ok) {
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
  SerialRFID.write(ReadMulti,10);
  Serial.println("RFID: started ReadMulti.");
  Serial.println("RFID: running");
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
  if (epcString != lastEpcString || (lastEpcRead + RFID_REPEAT_TIME) < now) {
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
      Serial.println("Select command sent.");
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
          ledOn(WEBSOCKET_LED_PIN);
          wait(200);
          ledOff(RFID_LED_PIN);
          ledOff(WEBSOCKET_LED_PIN);
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

void ledOn(const int led_pin) {
  #ifdef INVERT_LEDS
    digitalWrite(led_pin, HIGH);
  #else
    digitalWrite(led_pin, LOW);
  #endif
  if(led_pin == RFID_LED_PIN) {
    RfidLedOnMs = millis();
  }
}

void ledOff(const int led_pin) {
  #ifdef INVERT_LEDS
    digitalWrite(led_pin, LOW);
  #else
    digitalWrite(led_pin, HIGH);
  #endif
  if(led_pin == RFID_LED_PIN) {
    RfidLedOnMs = 0;
  }
}

void setup() {
  preferences.begin(PREFERENCES_NAMESPACE, false);

  pinMode(RFID_LED_PIN, OUTPUT);
  ledOn(RFID_LED_PIN);

  pinMode(WEBSOCKET_LED_PIN, OUTPUT);
  ledOn(WEBSOCKET_LED_PIN);

  pinMode(WIFI_AP_LED_PIN, OUTPUT);
  ledOn(WIFI_AP_LED_PIN);
  delay(1000);
  ledOff(RFID_LED_PIN);
  ledOff(WEBSOCKET_LED_PIN);
  ledOff(WIFI_AP_LED_PIN);

  #ifdef PUSH_BUTTON_PIN
    pinMode(PUSH_BUTTON_PIN, INPUT_PULLUP);
  #endif

  //init rfid storage
  resetRfidStorage();
  delay(2000);

  Serial.begin(115200);
  wait(2000);

  Serial.print("RFID-Connector Version: ");
  Serial.println(VERSION);
  Serial.println("############################");

  configurationLoad();

  Serial.println();
  Serial.println("Starting ...");

  if (wifiSsid != "") {
    WiFi.setHostname(wifiHostname.c_str());
    WiFi.begin(wifiSsid.c_str(), wifiPassword.c_str());

    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < WIFI_CONNECT_ATTEMPTS) {
      waitForWifi(WIFI_CONNECT_DELAY_MS);
      Serial.print(".");
      attempts++;
    }

    if (WiFi.status() == WL_CONNECTED) {
      Serial.print("\nWiFi: connected ");
      Serial.println(WiFi.localIP());
      Serial.print("WiFi: Hostname: ");
      Serial.println(WiFi.getHostname());
      wifiApMode = false;
      ledOff(WIFI_AP_LED_PIN);
    } else {
      WiFi.disconnect(true);
      wait(500);
      Serial.println("\nWiFi: connection failed!");
      WiFi.softAP(WIFI_AP_SSID);
      dnsServer.start();
      Serial.println("WiFi: started AP mode");
      wifiApMode = true;
      ledOn(WIFI_AP_LED_PIN);
    }
  } else {
    WiFi.softAP(WIFI_AP_SSID);
    dnsServer.start();
    Serial.println("WiFi: started AP mode");
    wifiApMode = true;
    ledOn(WIFI_AP_LED_PIN);
  }

  server.on("/", handleRoot);
  server.on("/label-writer", handleLabelWriter);
  server.on("/write-epc", HTTP_POST, handleWriteEpc);
  server.on("/config", HTTP_POST, handleConfig);
  // all unknown pages are redirected to configuration page
  server.onNotFound(handleNotFound);

  server.begin();
  Serial.println("Webserver: running");
  webserverRunning = true;

  // Start RFID reader
  initRfid();
}

void loop() {
  server.handleClient();
  readTag = readRfid();
  client.poll();
  now = millis();

  if(!websocketConnected){
    if(!wifiApMode) connectWebsocket();
  }
  else if(now > (websocketLastPing + WEBSOCKET_PING_INTERVAL)) {
    websocketLastPing = now;
    client.ping();
  }

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
}
