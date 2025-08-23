#ifndef CONFIG_H
#define CONFIG_H

#define VERSION "1.4.2" // dont forget to update the releases.json

//#define DEBUG // enable debug output to Serial Monitor

//#define STARTING_LIGHTS // define if you want to use the starting lights functionality

#ifdef STARTING_LIGHTS
  #define WIFI_DEFAULT_HOSTNAME "starting-lights"
#else
  #define WIFI_DEFAULT_HOSTNAME "ghostcar"
#endif

#define KEY_TURBO 3 // Turbo-Button at gamepad
#define DEFAULT_SPEED 75 // ghostcar speed %
#define DEFAULT_BRAKING_TIME 5000 // ms
#define JOYSTICK_UPDATE_INTERVAL 100 // ms

// USB CDC on boot should be enabled in the Arduino IDE
// For ESP32C3 or ESP32S3_BLE use Tools -> Partition Scheme -> Huge App (3MB No OTA/1MB SPIFFS)
// Additional board managers (current ESP32 version) => https://espressif.github.io/arduino-esp32/package_esp32_index.json
// Include libraries from the libs folder via Arduino IDE (Zip)

//#define ESP32C3 // only Bluetooth
#define ESP32S3 // Bluetooth and USB-Joystik

// Do not change the following lines!
#ifdef ESP32S3
  #define RGB_LED // optional - RGB LED is used for feedback
  #define RGB_LED_PIN 21
  #define START_BUTTON_PIN 10 // optional - Start button for ghostcar - should not be commented out!
  #define STOP_BUTTON_PIN 11 // optional - Stop button for ghostcar - should not be commented out!
  //#define SPEED_POT_PIN 3 // optional - Speed-Potentiometer for ghostcar speed
#elif defined(ESP32C3)
  #define WIFI_AP_LED_PIN 8 // optional - LED is used for feedback
  #define WEBSOCKET_LED_PIN 9 // optional - LED is used for feedback
  #define START_BUTTON_PIN 6 // optional - Start button for ghostcar - should not be commented out!
  #define STOP_BUTTON_PIN 7 // optional - Stop button for ghostcar - should not be commented out!
  //#define SPEED_POT_PIN 3 // optional - Speed-Potentiometer for ghostcar speed
#endif

// define the LED type
//#define LED_TYPE WS2811 // LED type, e.g. WS2811
#define LEDTYPE WS2812B // LED type, e.g. WS2812B

// define the FastLED pin
#define FAST_LED_PIN 5 // Pin for FastLED, e.g. GPIO 5

#define DEFAULT_BRIGHTNESS 150 // default brightness for the LEDs, can be changed in the web interface

// define colors for the starting lights
#define RED CRGB::Red // Green and Red may be swapped
#define GREEN CRGB::Green
#define YELLOW CRGB::Yellow
#define BLUE CRGB::Blue
#define WHITE CRGB::White

// select the starting lights version
//#define STARTING_LIGHTS_VERSION_3_ROWS
#define STARTING_LIGHTS_VERSION_4_ROWS

#ifdef STARTING_LIGHTS_VERSION_1_ROWS
  #define NUM_LEDS 5
  #define LED_ROWS 1

  const int websocketLedRows[] = {1};

  const int wifiLedRows[] =  {1};

  #define CH_RACING_CLUB_LEDS_COUNTDOWN_TIME 5000
  #define CH_RACING_CLUB_LEDS_FLASH_INTERVAL 500
  const int chRacingClubDriveLedRows[] = {1};
  const int chRacingClubStopLedRows[] = {1};
  const int chRacingClubYellowLedRows[] = {1};
  const int chRacingClubCountdownLedRows[] = {1};

  #define SMARTRACE_LEDS_COUNTDOWN_TIME 5000
  #define SMARTRACE_LEDS_FLASH_INTERVAL 600
  const int smartraceDriveLedRows[] = {1, 2};
  const int smartraceStopLedRows[] = {1, 2};
  const int smartraceYellowLedRows[] = {1, 2};
  const int smartraceCountdownLedRows[] = {1, 2};
#endif // STARTING_LIGHTS_VERSION_1_ROW

#ifdef STARTING_LIGHTS_VERSION_2_ROWS
  #define NUM_LEDS 10
  #define LED_ROWS 2

  const int websocketLedRows[] = {1, 2};

  const int wifiLedRows[] =  {1, 2};

  #define CH_RACING_CLUB_LEDS_COUNTDOWN_TIME 5000
  #define CH_RACING_CLUB_LEDS_FLASH_INTERVAL 500
  const int chRacingClubDriveLedRows[] = {1, 2};
  const int chRacingClubStopLedRows[] = {1, 2};
  const int chRacingClubYellowLedRows[] = {1, 2};
  const int chRacingClubCountdownLedRows[] = {1, 2};

  #define SMARTRACE_LEDS_COUNTDOWN_TIME 5000
  #define SMARTRACE_LEDS_FLASH_INTERVAL 600
  const int smartraceDriveLedRows[] = {1, 2};
  const int smartraceStopLedRows[] = {1, 2};
  const int smartraceYellowLedRows[] = {1, 2};
  const int smartraceCountdownLedRows[] = {1, 2};
#endif // STARTING_LIGHTS_VERSION_2_ROW

#ifdef STARTING_LIGHTS_VERSION_3_ROWS
  #define NUM_LEDS 15
  #define LED_ROWS 3

  const int websocketLedRows[] = {1, 3};

  const int wifiLedRows[] =  {1, 3};

  #define CH_RACING_CLUB_LEDS_COUNTDOWN_TIME 5000
  #define CH_RACING_CLUB_LEDS_FLASH_INTERVAL 500
  const int chRacingClubDriveLedRows[] = {3};
  const int chRacingClubStopLedRows[] = {1, 2};
  const int chRacingClubYellowLedRows[] = {1};
  const int chRacingClubCountdownLedRows[] = {1, 2};

  #define SMARTRACE_LEDS_COUNTDOWN_TIME 5000
  #define SMARTRACE_LEDS_FLASH_INTERVAL 600
  const int smartraceDriveLedRows[] = {3};
  const int smartraceStopLedRows[] = {1, 2};
  const int smartraceYellowLedRows[] = {1};
  const int smartraceCountdownLedRows[] = {1, 2};
#endif // STARTING_LIGHTS_VERSION_3_ROW

#ifdef STARTING_LIGHTS_VERSION_4_ROWS
  #define NUM_LEDS 20
  #define LED_ROWS 4

  const int websocketLedRows[] = {1, 4};

  const int wifiLedRows[] =  {1, 4};

  #define CH_RACING_CLUB_LEDS_COUNTDOWN_TIME 5000
  #define CH_RACING_CLUB_LEDS_FLASH_INTERVAL 300
  const int chRacingClubDriveLedRows[] = {3, 4};
  const int chRacingClubStopLedRows[] = {3, 4};
  const int chRacingClubYellowLedRows[] = {1, 2};
  const int chRacingClubCountdownLedRows[] = {3, 4};

  #define SMARTRACE_LEDS_COUNTDOWN_TIME 5000
  #define SMARTRACE_LEDS_FLASH_INTERVAL 300
  const int smartraceDriveLedRows[] = {3, 4};
  const int smartraceStopLedRows[] = {1, 2};
  const int smartraceYellowLedRows[] = {1, 2};
  const int smartraceCountdownLedRows[] = {1, 2, 3, 4};
#endif // STARTING_LIGHTS_VERSION_4_ROWS

#endif // CONFIG_H
