#include "StartingLights.h" //Needs FastLED by Daniel Garcia
#include "config.h"
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <Preferences.h>
#include <ArduinoWebsockets.h> //ArduinoWebsockets 0.5.4
#include <ArduinoJson.h> // Tested V0.9.5

#ifndef ESP32C3
  #include <Joystick_ESP32S2.h> //https://github.com/schnoog/Joystick_ESP32S2
#endif
#include "src/Joystick_BLE/Joystick_BLE.h"

/* configuration */
#ifdef STARTING_LIGHTS
  #define WIFI_AP_SSID "StartingLights-Config"
  #define PRODUCT_NAME "StartingLights"
#else
  #define WIFI_AP_SSID "GhostCar-Config"
  #define PRODUCT_NAME "GhostCar"
#endif
#define PREFERENCES_NAMESPACE "racebox"

Joystick_BLE_ Joystick_BLE(PRODUCT_NAME);

#ifndef ESP32C3
  Joystick_ Joystick(JOYSTICK_DEFAULT_REPORT_ID,JOYSTICK_TYPE_GAMEPAD,
                   14, 2,true, false, false, false, false,
                   false,false, false, true, true, false);
#endif

#define WIFI_CONNECT_ATTEMPTS 10
#define WIFI_CONNECT_DELAY_MS 1000
#define WEBSOCKET_PING_INTERVAL 5000

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

bool activeLaunchControl = false;
bool isBraking = false;
bool isDriving = false;
unsigned long brakeTime = 0;
unsigned long lastJoystickUpdate = 0;

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

int speed = DEFAULT_SPEED; // ghostcar speed %
int ledBrightness = DEFAULT_BRIGHTNESS; // LED brightness
int controllerId; // Controller ID for SmartRace
bool startButtonPressed = false;
bool stopButtonPressed = false;

StartingLights startingLights(NUM_LEDS, LED_ROWS);

const int websocketLedNumRows = sizeof(websocketLedRows) / sizeof(websocketLedRows[0]);
const int wifiLedNumRows = sizeof(wifiLedRows) / sizeof(wifiLedRows[0]);
const int chRacingClubDriveLedNumRows = sizeof(chRacingClubDriveLedRows) / sizeof(chRacingClubDriveLedRows[0]);
const int chRacingClubStopLedNumRows = sizeof(chRacingClubStopLedRows) / sizeof(chRacingClubStopLedRows[0]);
const int chRacingClubYellowLedNumRows = sizeof(chRacingClubYellowLedRows) / sizeof(chRacingClubYellowLedRows[0]);
const int chRacingClubCountdownLedNumRows = sizeof(chRacingClubCountdownLedRows) / sizeof(chRacingClubCountdownLedRows[0]);
const int smartraceDriveLedNumRows = sizeof(smartraceDriveLedRows) / sizeof(smartraceDriveLedRows[0]);
const int smartraceStopLedNumRows = sizeof(smartraceStopLedRows) / sizeof(smartraceStopLedRows[0]);
const int smartraceYellowLedNumRows = sizeof(smartraceYellowLedRows) / sizeof(smartraceYellowLedRows[0]);
const int smartraceCountdownLedNumRows = sizeof(smartraceCountdownLedRows) / sizeof(smartraceCountdownLedRows[0]);

void configurationSave() {
  preferences.putString("target_system", targetSystem);

  preferences.putString("wifi_ssid", wifiSsid);
  preferences.putString("wifi_password", wifiPassword);
  preferences.putString("wifi_hostname", wifiHostname);

  preferences.putString("sr_ws_server", smartRaceWebsocketServer);

  preferences.putString("chrc_ws_server", chRacingClubWebsocketServer);
  preferences.putString("chrc_ws_ca_cert", chRacingClubCaCert);
  preferences.putString("chrc_api_key", chRacingClubApiKey);
  
  #ifndef SPEED_POT_PIN
    preferences.putInt("speed", speed);
  #endif
  #ifdef STARTING_LIGHTS
    preferences.putInt("led_brightness", ledBrightness);
  #endif
  preferences.putInt("controller_id", controllerId);
}

void configurationLoad() {
  targetSystem = preferences.getString("target_system", "smart_race");

  wifiSsid = preferences.getString("wifi_ssid", "");
  wifiPassword = preferences.getString("wifi_password", "");
  wifiHostname = preferences.getString("wifi_hostname", WIFI_DEFAULT_HOSTNAME);

  smartRaceWebsocketServer = preferences.getString("sr_ws_server", "");
  
  chRacingClubWebsocketServer = preferences.getString("chrc_ws_server", "");
  chRacingClubCaCert = preferences.getString("chrc_ws_ca_cert", "");
  chRacingClubApiKey = preferences.getString("chrc_api_key", "");
  #ifndef SPEED_POT_PIN
    speed = preferences.getInt("speed", DEFAULT_SPEED);
  #endif
  #ifdef STARTING_LIGHTS
    startingLights.setBrightness(preferences.getInt("led_brightness", 150));
  #endif
  controllerId = preferences.getInt("controller_id", 1);
  if (targetSystem == "smart_race") {
    websocketServer = smartRaceWebsocketServer;
    websocketCaCert = "";
    #ifdef ESP32C3
      Serial.println("\nConfiguration: SmartRace loaded");
    #endif
  }

  if (targetSystem == "ch_racing_club") {
    websocketServer = chRacingClubWebsocketServer;
    websocketCaCert = chRacingClubCaCert;
    #ifdef ESP32C3
      Serial.println("\nConfiguration: CH Racing Club loaded");
    #endif
  }
}

void handleNotFound() {
  server.sendHeader("Location", "/");
  server.send(302, "text/plain", "redirect to configuration page");
}

void handleRoot() {
  String html = String("<!DOCTYPE html><html><head><title>") + PRODUCT_NAME + "</title>";
  html += "<meta charset='UTF-8'>"; // Specify UTF-8 encoding
  html += "<style>";
  html += "*, *::before, *::after {box-sizing: border-box;}";
  html += "body {min-height: 100vh;margin: 0;}";
  html += "form {max-width: 535px;margin: 0 auto;}";
  html += "label {margin-bottom: 5px;display:block;}";
  html += "input[type=text],input[type=password],input[type=number],select,textarea {width: 100%;padding: 8px;border: 1px solid #ccc;border-radius: 4px;display: block;}";
  html += "input[type=submit] {width: 100%;background-color: #4CAF50;color: white;padding: 10px 15px;border: none;border-radius: 4px;}";
  html += "</style>";
  html += "<script src='https://unpkg.com/alpinejs' defer></script>";
  html += "</head><body>";
  html += "<form x-data=\"{ targetSystem: '" + targetSystem + "', smartRaceWebsocketServer: '" + smartRaceWebsocketServer + "', racingClubWebsocketServer: '" + chRacingClubWebsocketServer + "' }\" action='/config' method='POST'>";
  html += String("<h1 align=center>") + PRODUCT_NAME + "</h1>";
  html += String("<h5 align=center>[") + Joystick_BLE.getDeviceName() + "]</h5>";
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
    html += "  <label for='controller_id'>Controller ID (1-6):</label>";
    html += "  <input type='number' id='controller_id' name='controller_id' value='" + String(controllerId) + "' min='1' max='6'><br>";
    html += "</div>";
    #ifdef STARTING_LIGHTS
      html += "<label for='led_brightness'>LED Brightness (0-255):</label>";
      html += "<input type='number' id='led_brightness' name='led_brightness' value='" + String(ledBrightness) + "' min='0' max='255'><br>";
    #endif
    #ifndef SPEED_POT_PIN
      html += "<label for='speed'>GhostCar speed (%):</label>";
      html += "<input type='number' id='speed' name='speed' value='" + String(speed) + "' min='10' max='100'><br>";
    #endif
  }

  html += "<input type='submit' style='margin-bottom:20px;' value='Speichern'>";
  html += "</form>";
  html += "</body></html>";
  server.send(200, "text/html", html);
}

void handleConfig() {
  if (server.args() > 0) {
    bool reConnectWifi = wifiSsid != server.arg("wifi_ssid") || wifiPassword != server.arg("wifi_password") || wifiHostname != server.arg("wifi_hostname");
    bool reConnectWebsocket = false;

    wifiSsid = server.arg("wifi_ssid");
    wifiPassword = server.arg("wifi_password");
    wifiHostname = server.arg("wifi_hostname");

    if(!wifiApMode) {
      reConnectWebsocket = targetSystem != server.arg("target_system");

      targetSystem = server.arg("target_system");
      if (targetSystem == "smart_race") {
        if (smartRaceWebsocketServer != server.arg("smart_race_websocket_server")) {
          reConnectWebsocket = true; // Reconnect if the server has changed
          smartRaceWebsocketServer = server.arg("smart_race_websocket_server");
        }
        if (controllerId != server.arg("controller_id").toInt()) {
          reConnectWebsocket = true; // Reconnect if the server has changed
          controllerId = server.arg("controller_id").toInt();
          if(controllerId > 6 || controllerId < 1) {
            controllerId = 1; // Reset to default if out of range
          }
        }
        websocketServer = smartRaceWebsocketServer;
        websocketCaCert = ""; // SmartRace does not use a CA cert
      }

      if (targetSystem == "ch_racing_club") {
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
      }
      #ifdef STARTING_LIGHTS
        ledBrightness = server.arg("led_brightness").toInt();
        if (ledBrightness < 0) {
          ledBrightness = 0;
        } else if (ledBrightness > 255) {
          ledBrightness = 255;
        }
        startingLights.setBrightness(ledBrightness);
      #endif
      #ifndef SPEED_POT_PIN
        speed = server.arg("speed").toInt();
        if (speed > 100) {
          speed = 100;
        }
        else if (speed < 10) {
          speed = 10;
        }
        if(isDriving) {
          Joystick_BLE.setAccelerator(speed);
          Joystick_BLE.setAccelerator(speed);
          Joystick_BLE.sendState();
          #ifndef ESP32C3
            Joystick.setAccelerator(speed);
            Joystick.setAccelerator(speed);
            Joystick.sendState();
          #endif
        }
      #endif
    }
    configurationSave();

    server.send(200, "text/html", String("<!DOCTYPE html><html><head><title>") + PRODUCT_NAME + "</title></head><body><h1>Configuration saved!</h1><p>You will be redirected in 2 seconds.</p><script>setTimeout(function() { window.location.href = 'http://" + wifiHostname + "'; }, 2000);</script></body></html>");
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
  } else {
    server.send(200, "text/html", String("<!DOCTYPE html><html><head><title>")+ PRODUCT_NAME + "</title></head><body><h1>Invalid request!</h1><p>You will be redirected in 2 seconds.</p><script>setTimeout(function() { window.location.href = 'http://" + wifiHostname + "'; }, 2000);</script></body></html>");
  }
}

void wifiReload() {
  #ifdef ESP32C3
    Serial.println("WiFi: Initiating reload...");
  #endif
  if (WiFi.getMode() != WIFI_OFF) {
    #ifdef ESP32C3
      Serial.println("WiFi: Disconnecting existing connections...");
    #endif
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
  #ifdef ESP32C3
    Serial.print("WiFi: Attempting to connect to AP '");
    Serial.print(wifiSsid);
    Serial.println("'...");
  #endif
  WiFi.mode(WIFI_STA);
  WiFi.begin(wifiSsid.c_str(), wifiPassword.c_str());

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < WIFI_CONNECT_ATTEMPTS) {
    waitForWifi(WIFI_CONNECT_DELAY_MS);
    #ifdef ESP32C3
      Serial.print(".");
    #endif
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    #ifdef ESP32C3
      Serial.print("\nWiFi: connected ");
      Serial.println(WiFi.localIP());
      Serial.print("WiFi: Hostname: ");
      Serial.println(WiFi.getHostname());
    #endif
    wifiApMode = false;
    #ifdef WIFI_AP_LED_PIN
      ledOff(WIFI_AP_LED_PIN);
    #endif
    startingLights.stopRunningSequence();
    startingLights.setRowLights(wifiLedRows, wifiLedNumRows, WHITE);
    #ifdef RGB_LED
      rgbLedWrite(RGB_LED_PIN, 200, 200, 200);
    #endif
  } else {
    #ifdef ESP32C3
      Serial.println("\nWiFi: connect failed, starting AP mode");
    #endif
    WiFi.disconnect(true);
    wait(500);
    WiFi.softAP(WIFI_AP_SSID);
    dnsServer.start();
    wifiApMode = true;
    #ifdef WIFI_AP_LED_PIN
      ledOn(WIFI_AP_LED_PIN);
    #endif
    #ifdef ESP32C3
      Serial.print("\nWiFi: AP started, IP: ");
      Serial.println(WiFi.softAPIP());
    #endif
    startingLights.stopRunningSequence();
    startingLights.setRowLights(wifiLedRows, wifiLedNumRows, RED);
    #ifdef RGB_LED
      rgbLedWrite(RGB_LED_PIN, 0, 255, 0);
    #endif
  }
}

void connectWebsocket() {
  if (millis() - websocketLastAttempt < websocketBackoff) {
    return; // wait until backoff time is reached
  }
  websocketLastAttempt = millis();

  client = WebsocketsClient(); // Überschreibt das alte Objekt

  client.onMessage(onMessageCallback);
  client.onEvent(onEventsCallback);

  #ifdef ESP32C3
    Serial.print("Websocket: connecting ");
    Serial.println(websocketServer);
  #endif

  if (websocketCaCert.length() > 0) {
    client.setCACert(websocketCaCert.c_str());
  }
  websocketConnected = client.connect(websocketServer);

  if(websocketConnected) {
    JsonDocument doc;
    if(targetSystem == "smart_race") {
      doc["type"] = "controller_set";
      doc["data"]["controller_id"] = String(controllerId);
    }

    if(targetSystem == "ch_racing_club") {
      #ifdef STARTING_LIGHTS
        doc["command"] = "starting_light_connect";        
        doc["data"]["name"] = wifiHostname.c_str();
      #else
        doc["command"] = "ghostcar_connect";
        doc["data"]["name"] = Joystick_BLE.getDeviceName();
      #endif
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
    #ifdef ESP32C3
      Serial.println("Websocket: connection failed.");
    #endif
    if (targetSystem == "ch_racing_club") {
      websocketBackoff = min(websocketBackoff * 2, websocketMaxBackoff); // Exponentielles Backoff
    }
    else {
      websocketBackoff = 3000; // set backoff time for SmartRace
    }
  }
}

void onMessageCallback(WebsocketsMessage message) {
  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, message.data());
  if (error) {
    #ifdef ESP32C3
      Serial.print("Websocket: JSON deserialization failed: ");
      Serial.println(error.c_str());
    #endif
    return;
  }

  #if defined(DEBUG) && defined(ESP32C3)
    Serial.print("Websocket: received message: ");
    serializeJsonPretty(doc, Serial);
    Serial.println();
  #endif
  if (targetSystem == "ch_racing_club") {
    JsonDocument payload;
    DeserializationError error = deserializeJson(payload, doc["message"].as<String>());

    //uses samge message format as SmartRace so far...
    if (payload.containsKey("command")) {
      handleRacingClubUpdateEvent(payload["command"].as<String>(), payload);
    } else {
      #ifdef ESP32C3
        Serial.println("Received message without 'command' key: " + payload.as<String>());
      #endif
    }
  } else {
    if (doc.containsKey("type")) {
      handleSmartRaceUpdateEvent(doc["type"].as<String>(), doc);
    } else {
      #ifdef ESP32C3
        Serial.println("Received message without 'type' key: " + message.data());
      #endif
    }
  }
}

void handleRacingClubUpdateEvent(String command, JsonDocument doc) {
  String api_key = doc["api_key"].as<String>();
  String status = doc["status"].as<String>();

  // received message with invalid API key
  if (api_key != chRacingClubApiKey) {
    return;
  }

  if (command == "launchcontrol") {
    #if defined(DEBUG) && defined(ESP32C3)
      Serial.println("INFO - starting");
    #endif
    startingLights.stopRunningSequence();
    startingLights.runCountDownLights(chRacingClubCountdownLedRows, chRacingClubCountdownLedNumRows, CH_RACING_CLUB_LEDS_COUNTDOWN_TIME, RED);
    activateLaunchControl();
  } else if (command == "drive") {
    #if defined(DEBUG) && defined(ESP32C3)
      Serial.println("INFO - running");
    #endif
    startingLights.stopRunningSequence();
    startingLights.setRowLights(chRacingClubDriveLedRows, chRacingClubDriveLedNumRows, GREEN);
    drive();
  } else if (command == "stop") {
    #if defined(DEBUG) && defined(ESP32C3)
      Serial.println("INFO - stop");
    #endif
    stop();
    if (status == "ended") {
      startingLights.stopRunningSequence();
      startingLights.runFlashLights(chRacingClubStopLedRows, chRacingClubStopLedNumRows, CH_RACING_CLUB_LEDS_FLASH_INTERVAL, RED, -1);
    } else if (status == "finished") {
      startingLights.stopRunningSequence();
      startingLights.runFlashLights(chRacingClubStopLedRows, chRacingClubStopLedNumRows, CH_RACING_CLUB_LEDS_FLASH_INTERVAL, RED, -1);
    } else if (status == "suspended") {
      startingLights.stopRunningSequence();
      startingLights.runFlashLights(chRacingClubYellowLedRows, chRacingClubYellowLedNumRows, CH_RACING_CLUB_LEDS_FLASH_INTERVAL, YELLOW, -1);
    } else if (status == "false_start") {
      startingLights.stopRunningSequence();
      startingLights.runFlashLights(chRacingClubStopLedRows, chRacingClubStopLedNumRows, CH_RACING_CLUB_LEDS_FLASH_INTERVAL, RED, -1);
    } else if (status == "idle") {
      startingLights.setAllLightsOff();
    } else {
      startingLights.setAllLightsOff();
    }
  }
}

void handleSmartRaceUpdateEvent(String type, JsonDocument doc) {
  if (type == "update_event_status" && doc.containsKey("data")) {
    String data = doc["data"].as<String>();
    if (data == "prepare_for_start") {
      #if defined(DEBUG) && defined(ESP32C3)
        Serial.println("INFO - prepare_for_start");
      #endif
      startingLights.runFlashLights(smartraceYellowLedRows, smartraceYellowLedNumRows, SMARTRACE_LEDS_FLASH_INTERVAL, YELLOW);
    } else if (data == "starting") {
      #if defined(DEBUG) && defined(ESP32C3)
        Serial.println("INFO - starting");
      #endif
      activateLaunchControl();
      startingLights.stopRunningSequence();
      startingLights.runCountDownLights(smartraceCountdownLedRows, smartraceCountdownLedNumRows, SMARTRACE_LEDS_COUNTDOWN_TIME, RED);
    } else if (data == "running") {
      #if defined(DEBUG) && defined(ESP32C3)
        Serial.println("INFO - running");
      #endif
      startingLights.stopRunningSequence();
      startingLights.setRowLights(smartraceDriveLedRows, smartraceDriveLedNumRows, GREEN);
      drive();
    } else if (data == "suspended") {
      #if defined(DEBUG) && defined(ESP32C3)
        Serial.println("INFO - suspended");
      #endif
      stop();
      startingLights.stopRunningSequence();
      startingLights.runFlashLights(smartraceStopLedRows, smartraceStopLedNumRows, SMARTRACE_LEDS_FLASH_INTERVAL, RED, -1);
    } else if (data == "ended") {
      #if defined(DEBUG) && defined(ESP32C3)
        Serial.println("INFO - ended");
      #endif
      stop();
      startingLights.stopRunningSequence();
      startingLights.runFlashLights(smartraceStopLedRows, smartraceStopLedNumRows, SMARTRACE_LEDS_FLASH_INTERVAL, RED, -1);
      } else {
      #if defined(DEBUG) && defined(ESP32C3)
        Serial.println("Unknown message type: " + type);
      #endif
    }
  } else if(type == "update_vsc_status") {
    String data = doc["data"].as<String>();
    if (data == "active") {
      #if defined(DEBUG) && defined(ESP32C3)
        Serial.println("INFO - vsc active");
      #endif
      stop();
      startingLights.stopRunningSequence();
      startingLights.runFlashLights(smartraceYellowLedRows, smartraceYellowLedNumRows, SMARTRACE_LEDS_FLASH_INTERVAL, YELLOW, -1);
    } else if (data == "off") {
      #if defined(DEBUG) && defined(ESP32C3)
        Serial.println("INFO - vsc off");
      #endif
      startingLights.stopRunningSequence();
      startingLights.setRowLights(smartraceDriveLedRows, smartraceDriveLedNumRows, GREEN);
      drive();
    }
  } else if (type == "reset") {
    #if defined(DEBUG) && defined(ESP32C3)
      Serial.println("INFO - reset");
    #endif
    stop();
    startingLights.stopRunningSequence();
    startingLights.setAllLightsOff();
  } else {
    #if defined(DEBUG) && defined(ESP32C3)
      Serial.println("Unknown message type: " + type);
    #endif
  }
}

void onEventsCallback(WebsocketsEvent event, String data) {
  if(event == WebsocketsEvent::ConnectionOpened) {
    #ifdef ESP32C3
      Serial.println("Websocket: connected");
    #endif
    
    websocketConnected = true;
    #ifdef WEBSOCKET_LED_PIN
      ledOn(WEBSOCKET_LED_PIN);
    #endif
    #ifdef RGB_LED
      rgbLedWrite(RGB_LED_PIN, 0, 0, 255);
    #endif
    startingLights.stopRunningSequence();
    startingLights.setRowLights(websocketLedRows, websocketLedNumRows, BLUE);
  } else if(event == WebsocketsEvent::ConnectionClosed) {
    #ifdef ESP32C3
      Serial.println("Websocket: connection closed");
    #endif
    websocketConnected = false;
    #ifdef WEBSOCKET_LED_PIN
      ledOff(WEBSOCKET_LED_PIN);
    #endif
    #ifdef RGB_LED
      rgbLedWrite(RGB_LED_PIN, 200, 200, 200);
    #endif
    startingLights.stopRunningSequence();
    startingLights.setRowLights(websocketLedRows, websocketLedNumRows, WHITE);
  } else if(event == WebsocketsEvent::GotPing) {
     #if defined(DEBUG) && defined(ESP32C3)
      Serial.println("Websocket: got a ping!");
    #endif
    client.pong();
  } else if(event == WebsocketsEvent::GotPong) {
     #if defined(DEBUG) && defined(ESP32C3)
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
    startingLights.updateLeds();
  }
}

void waitForWifi(unsigned long waitTime) {
  unsigned long startWaitTime = millis();
  while((millis() - startWaitTime) < waitTime) {
    if(WiFi.status() == WL_CONNECTED) return; // exit if WiFi is connected
    startingLights.updateLeds();
    delay(10);
  }
}

void activateLaunchControl() {
   #if defined(DEBUG) && defined(ESP32C3)
    Serial.println("INFO - activate launch control");
  #endif
  isDriving = false;
  activeLaunchControl = true;
  isBraking = false;
  resetJoystickPosition();
  Joystick_BLE.setButton(KEY_TURBO, 1);
  Joystick_BLE.setAccelerator(speed);
  Joystick_BLE.sendState();
  #ifndef ESP32C3
    Joystick.setButton(KEY_TURBO, 1);
    Joystick.setAccelerator(speed);
    Joystick.sendState();
  #endif
  #ifdef RGB_LED
    rgbLedWrite(RGB_LED_PIN, 255, 255, 0);
  #endif
}

void drive() {
  #if defined(DEBUG) && defined(ESP32C3)
    Serial.println("INFO - ghostcar driving");
  #endif
  isBraking = false;
  if(activeLaunchControl) {
    Joystick_BLE.setBrake(0);
    Joystick_BLE.setButton(KEY_TURBO, 0);
    #ifndef ESP32C3
      Joystick.setBrake(0);
      Joystick.setButton(KEY_TURBO, 0);
    #endif
  }
  else {
    resetJoystickPosition();
    Joystick_BLE.setAccelerator(speed);
    #ifndef ESP32C3
      Joystick.setAccelerator(speed);
    #endif
  }
  Joystick_BLE.sendState();
  #ifndef ESP32C3
    Joystick.sendState();
  #endif
  activeLaunchControl = false;
  isDriving = true;
  #ifdef RGB_LED
    rgbLedWrite(RGB_LED_PIN, 255, 0, 0);
  #endif
}

void stop() {
  isDriving = false;
  #if defined(DEBUG) && defined(ESP32C3)
    Serial.println("INFO - ghostcar stopped");
  #endif
  resetJoystickPosition();
  Joystick_BLE.setBrake(100);
  Joystick_BLE.sendState();
  #ifndef ESP32C3
    Joystick.setBrake(100);
    Joystick.sendState();
  #endif
  brakeTime = millis();
  isBraking = true;
  activeLaunchControl = false;
  #ifdef RGB_LED
    rgbLedWrite(RGB_LED_PIN, 0, 255, 0);
  #endif
}

void initializeJoystickMode() {
  #ifdef ESP32S3
    USB.PID(0x8211);
    USB.VID(0x303b);
    USB.productName(PRODUCT_NAME);
    USB.manufacturerName("CH-Community");
    USB.begin();
    delay(100);
  #endif
  Joystick_BLE.setXAxisRange(-100, 100);
  Joystick_BLE.setAcceleratorRange(0, 100);
  Joystick_BLE.setBrakeRange(0, 100);
  Joystick_BLE.begin(false);
  #ifndef ESP32C3
    Joystick.setXAxisRange(-100, 100);
    Joystick.setAcceleratorRange(0, 100);
    Joystick.setBrakeRange(0, 100);
    Joystick.begin(false);
  #endif
  wait(500);
  resetJoystickPosition();
}

void resetJoystickPosition() {
  Joystick_BLE.setXAxis(0L);
  Joystick_BLE.setAccelerator(0L);
  Joystick_BLE.setBrake(0L);
  Joystick_BLE.setButton(KEY_TURBO, 0L);
  Joystick_BLE.sendState();
  #ifndef ESP32C3
    Joystick.setXAxis(0L);
    Joystick.setAccelerator(0L);
    Joystick.setBrake(0L);
    Joystick.setButton(KEY_TURBO, 0L);
    Joystick.sendState();
  #endif
  isDriving = false;
  isBraking = false;
}

void checkButtons() {
  startButtonPressed = digitalRead(START_BUTTON_PIN) == LOW;
  stopButtonPressed = digitalRead(STOP_BUTTON_PIN) == LOW;

  if(startButtonPressed || stopButtonPressed) {
    #ifdef ESP32C3
      Serial.println("Button pressed");
    #endif
    JsonDocument doc;
    if(startButtonPressed) {
      if(websocketConnected) {
        if(targetSystem == "smart_race") {
          if(isDriving) {
            doc["type"] = "race_control";
            doc["data"]["value"]  = "vsc";
          } else {
            doc["type"] = "race_control";
            doc["data"]["value"] = "start";
          }
        }
        else if(targetSystem == "ch_racing_club") {
          if(isDriving) {
            doc["command"] = "stop";
            doc["status"] = "ended";;
            doc["api_key"] = chRacingClubApiKey;
          } else {
            doc["command"] = "launchcontrol";
            doc["status"] = "starting";
            doc["api_key"] = chRacingClubApiKey;
            char output[256];
            serializeJson(doc, output);
            client.ping();
            client.send(output);
            wait(6000);
            doc["command"] = "drive";
            doc["status"] = "running";
          }
        }
      } else {
        drive();
      }
    }
    else if(stopButtonPressed) {
      if(websocketConnected) {
        if(targetSystem == "smart_race") {
          if(isDriving) {
            doc["type"] = "race_control";
            doc["data"]["value"] = "stop";
          } else {
            doc["type"] = "race_control";
            doc["data"]["value"] = "esc";
          }
        }
        else if(targetSystem == "ch_racing_club") {
          if(isDriving) {
            doc["command"] = "stop";
            doc["status"] = "suspended";
          } else {
            doc["command"] = "drive";
            doc["status"] = "running";
          }
          doc["api_key"] = chRacingClubApiKey;
        }
      } else {
        stop();
      }
    }
    if(websocketConnected) {
      char output[256];
      serializeJson(doc, output);
      client.ping();
      client.send(output);
    }
    while(digitalRead(START_BUTTON_PIN) == LOW || digitalRead(STOP_BUTTON_PIN) == LOW) {
      wait(100);
    }
  }
}

bool isLedOn(int led_pin) {
  if (digitalRead(led_pin) == LOW) {
    return true;
  }
  return false;
}

void ledOn(int led_pin) {
  digitalWrite(led_pin, LOW);
}

void ledOff(int led_pin) {
  digitalWrite(led_pin, HIGH);
}

void setup() {
  startingLights.begin(ledBrightness);
  preferences.begin(PREFERENCES_NAMESPACE, false);

  #ifdef WEBSOCKET_LED_PIN
    pinMode(WEBSOCKET_LED_PIN, OUTPUT);
    ledOn(WEBSOCKET_LED_PIN);
  #endif
  #ifdef WIFI_AP_LED_PIN
    pinMode(WIFI_AP_LED_PIN, OUTPUT);
    ledOn(WIFI_AP_LED_PIN);
  #endif
  delay(1000);
  #ifdef WEBSOCKET_LED_PIN
    ledOff(WEBSOCKET_LED_PIN);
  #endif
  #ifdef WIFI_AP_LED_PIN
    ledOff(WIFI_AP_LED_PIN);
  #endif
  #ifdef RGB_LED
    pinMode(RGB_LED_PIN, OUTPUT);
    rgbLedWrite(RGB_LED_PIN, 0, 0, 0);
  #endif

  #ifdef ESP32C3
    Serial.begin(115200);
    wait(2000);
    Serial.print(String(PRODUCT_NAME) + " Version: ");
    Serial.print(VERSION);
    Serial.println(" started.");
    Serial.println("############################");
  #endif
  configurationLoad();

  #ifdef ESP32C3
    Serial.println();
    Serial.println("Starting ...");
  #endif

  if (wifiSsid != "") {
    WiFi.setHostname(wifiHostname.c_str());
    WiFi.begin(wifiSsid.c_str(), wifiPassword.c_str());

    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < WIFI_CONNECT_ATTEMPTS) {
      waitForWifi(WIFI_CONNECT_DELAY_MS);
      #ifdef ESP32C3
        Serial.print(".");
      #endif
      attempts++;
    }

    if (WiFi.status() == WL_CONNECTED) {
      #ifdef ESP32C3
        Serial.print("\nWiFi: connected ");
        Serial.println(WiFi.localIP());
        Serial.print("WiFi: Hostname: ");
        Serial.println(WiFi.getHostname());
      #endif
      wifiApMode = false;
      #ifdef WIFI_AP_LED_PIN
        ledOff(WIFI_AP_LED_PIN);
      #endif
      startingLights.setRowLights(wifiLedRows, wifiLedNumRows, WHITE);
      #ifdef RGB_LED
        rgbLedWrite(RGB_LED_PIN, 200, 200, 200);
      #endif
    } else {
      WiFi.disconnect(true);
      wait(500);
      #ifdef ESP32C3
        Serial.println("\nWiFi: connection failed!");
      #endif
      WiFi.softAP(WIFI_AP_SSID);
      dnsServer.start();
      #ifdef ESP32C3
        Serial.println("WiFi: started AP mode");
      #endif
      wifiApMode = true;
      #ifdef WIFI_AP_LED_PIN
        ledOn(WIFI_AP_LED_PIN);
      #endif
      startingLights.setRowLights(wifiLedRows, wifiLedNumRows, RED);
      #ifdef RGB_LED
        rgbLedWrite(RGB_LED_PIN, 0, 255, 0);
      #endif
    }
  } else {
    WiFi.softAP(WIFI_AP_SSID);
    dnsServer.start();
    #ifdef ESP32C3
      Serial.println("WiFi: started AP mode");
    #endif
    wifiApMode = true;
    #ifdef WIFI_AP_LED_PIN
      ledOn(WIFI_AP_LED_PIN);
    #endif
    startingLights.setRowLights(wifiLedRows, wifiLedNumRows, RED);
    #ifdef RGB_LED
      rgbLedWrite(RGB_LED_PIN, 0, 255, 0);
    #endif
  }

  wait(2000);

  pinMode(START_BUTTON_PIN, INPUT_PULLUP);
  pinMode(STOP_BUTTON_PIN, INPUT_PULLUP);

  initializeJoystickMode();

  server.on("/", handleRoot);
  server.on("/config", HTTP_POST, handleConfig);
  // all unknown pages are redirected to configuration page
  server.onNotFound(handleNotFound);

  server.begin();
  #ifdef ESP32C3
    Serial.println("Webserver: running");
    Serial.print("BLE device name: ");
    Serial.println(Joystick_BLE.getDeviceName());
  #endif
  webserverRunning = true;
}

void loop() {
  startingLights.updateLeds();
  server.handleClient();
  client.poll();
  if(websocketConnected) {
    if(millis() > (websocketLastPing + WEBSOCKET_PING_INTERVAL)) {
      websocketLastPing = millis();
      client.ping();
    }
  }
  else {
    if(!wifiApMode) connectWebsocket();
  }
  
  checkButtons();

  if(millis() > (lastJoystickUpdate + JOYSTICK_UPDATE_INTERVAL)) {
    lastJoystickUpdate = millis();
    #ifdef SPEED_POT_PIN
      int newSpeed = map(analogRead(SPEED_POT_PIN), 0, 4095, 0, 100);
      if(newSpeed < 0) {
        newSpeed = 0;
      }
      if(newSpeed > 100) {
        newSpeed = 100;
      }
      if(newSpeed != speed) {
        speed = newSpeed;
        if(isDriving) {
          Joystick_BLE.setAccelerator(speed);
          #ifndef ESP32C3
            Joystick.setAccelerator(speed);
          #endif
        }
        #if defined(DEBUG) && defined(ESP32C3)
          Serial.print("Speed changed to: ");
          Serial.println(speed);
        #endif
      }
    #endif
    if(isBraking && (millis() > (brakeTime + DEFAULT_BRAKING_TIME))) {
      isBraking = false;
      Joystick_BLE.setBrake(0);
      #ifndef ESP32C3
        Joystick.setBrake(0);
      #endif
      #ifdef RGB_LED
        rgbLedWrite(RGB_LED_PIN, 200, 200, 200);
      #endif
    }
    Joystick_BLE.sendState();
    #ifndef ESP32C3
      Joystick.sendState();
    #endif
  }
}
