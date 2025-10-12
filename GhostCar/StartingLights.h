#ifndef STARTINGLIGHTS_H
#define STARTINGLIGHTS_H
#include <FastLED.h>
#include "config.h"

#ifndef LED_TYPE
  #define LED_TYPE WS2811 // Default LED type, can be overridden in config.h
#endif
#ifndef FAST_LED_PIN
  #define FAST_LED_PIN 5 // Default pin for FastLED, can be overridden in config.h
#endif
#ifndef LED_BRIGHTNESS
  #define LED_BRIGHTNESS 255 // Default LED brightness, can be overridden in config.h
#endif

enum LedMode {
    MODE_NONE,
    MODE_COUNTDOWN,
    MODE_FLASH
};

class StartingLights {
  public:
    StartingLights(int numLeds, int ledRows);
    ~StartingLights();
    void begin(int brightness);
    void setBrightness(int brightness);
    int getMatrixIndex(int row, int col) const;
    void setAllLightsOff();
    void setAllLights(CRGB color);
    void setRowLights(const int* rows, int numRows, CRGB color);
    void setColumnLights(int col, const int* rows, int numRows, CRGB color);
    void runCountDownLights(const int* rows, int numRows, unsigned int countDownTime, CRGB color);
    void runFlashLights(const int* rows, int numRows, unsigned int interval, CRGB color, int maxFlashCount = -1);
    bool isSequenceRunning(LedMode mode) const { return sequenceIsRunning && this->mode == mode; }
    void stopRunningSequence();
    void updateLeds();
  private:
    const int numLeds;
    const int ledRows;
    const int ledsPerRow;
    volatile bool sequenceIsRunning;
    std::vector<std::vector<int>> ledMatrix;
    CRGB* leds;

    // Für nicht-blockierende Logik:
    LedMode mode = MODE_NONE;
    // Countdown
    int countdownRows[8]; // max 8 Reihen, ggf. anpassen
    int countdownNumRows = 0;
    unsigned int countdownTime = 0;
    CRGB countdownColor;
    int countdownStep = 0;
    unsigned long countdownLastMillis = 0;
    // Flash
    int flashRows[8];
    int flashNumRows = 0;
    unsigned int flashInterval = 0;
    CRGB flashColor;
    int flashMaxCount = -1;
    int flashCount = 0;
    bool flashLedIsOn = true;
    unsigned long flashLastMillis = 0;
};

#endif
