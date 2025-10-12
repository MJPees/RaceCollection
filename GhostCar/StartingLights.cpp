#include "StartingLights.h"
#include <FastLED.h>

StartingLights::StartingLights(const int numLeds, int ledRows)
  : numLeds(numLeds), ledRows(ledRows), ledsPerRow(numLeds / ledRows) {
  leds = new CRGB[numLeds];
  sequenceIsRunning = false;
  // Build matrix mapping (custom pattern for 4x5 and 3x5)
  ledMatrix.resize(ledRows, std::vector<int>(ledsPerRow, -1));

  if (ledRows == 4 && ledsPerRow == 5 && numLeds == 20) {
    // 4x5 matrix, custom mapping
    int mapping[4][5] = {
      {  0,  7,  8, 15, 16 }, // row 0 (top)
      {  1,  6,  9, 14, 17 }, // row 1
      {  2,  5, 10, 13, 18 }, // row 2
      {  3,  4, 11, 12, 19 }  // row 3 (bottom)
    };
    for (int row = 0; row < 4; ++row) {
      for (int col = 0; col < 5; ++col) {
        ledMatrix[row][col] = mapping[row][col];
      }
    }
  } else if (ledRows == 3 && ledsPerRow == 5 && numLeds == 15) {
    // 3x5 matrix, custom mapping (update as needed)
    int mapping[3][5] = {
      { 0,  5,  6, 11, 12 }, // row 0 (top)
      { 1,  4,  7, 10, 13 }, // row 1
      { 2,  3,  8,  9, 14 }  // row 2 (bottom)
    };
    for (int row = 0; row < 3; ++row) {
      for (int col = 0; col < 5; ++col) {
        ledMatrix[row][col] = mapping[row][col];
      }
    } 
  } else if (ledRows == 2 && ledsPerRow == 5 && numLeds == 10) {
    // 2x5 matrix, custom mapping (update as needed)
    int mapping[2][5] = {
      { 0,  3,  4, 7, 8 }, // row 0 (top)
      { 1,  2,  5, 6, 9 }, // row 1
    };
    for (int row = 0; row < 2; ++row) {
      for (int col = 0; col < 5; ++col) {
        ledMatrix[row][col] = mapping[row][col];
      }
    }  
  } else if (ledRows == 1 && ledsPerRow == 5 && numLeds == 5) {
    // 1x5 matrix, custom mapping (update as needed)
    int mapping[1][5] = {
      { 0,  1,  2,  3,  4 }, // row 0 (top)
    };
    for (int col = 0; col < 5; ++col) {
      ledMatrix[0][col] = mapping[0][col];
    }
  } else {
    // Not supported
    // Optionally: set an error flag or print a warning
  }
}

StartingLights::~StartingLights() {
  delete[] leds;
}

int StartingLights::getMatrixIndex(int row, int col) const {
  if (row < 0 || row >= ledRows || col < 0 || col >= ledsPerRow) return -1;
  return ledMatrix[row][col];
}

void StartingLights::begin(int brightness) {
  FastLED.addLeds<LED_TYPE, FAST_LED_PIN, GRB>(leds, numLeds);
  if(brightness < 0 || brightness > 255) {
    brightness = 100;
  }
  setBrightness(brightness);
  setAllLightsOff();
}

void StartingLights::setBrightness(int brightness) {
  FastLED.setBrightness(brightness);
  FastLED.show();
}

void StartingLights::setAllLightsOff() {
  for (int i = 0; i < numLeds; i++) {
    leds[i] = CRGB::Black;
  }
  FastLED.show();
}

void StartingLights::setAllLights(CRGB color) {
  for (int i = 0; i < numLeds; i++) {
    leds[i] = color;
  }
  FastLED.show();
}

void StartingLights::setRowLights(const int* rows, int numRows, CRGB color) {
  for (int r = 0; r < numRows; r++) {
    int row = rows[r] - 1;
    if (row >= 0 && row < ledRows) {
      for (int col = 0; col < ledsPerRow; col++) {
        int idx = getMatrixIndex(row, col);
        if (idx >= 0 && idx < numLeds) {
          leds[idx] = color;
        }
      }
    }
  }
  FastLED.show();
}

void StartingLights::setColumnLights(int col, const int* rows, int numRows, CRGB color) {
  if (col < 0 || col >= ledsPerRow) return;
  for (int r = 0; r < numRows; r++) {
    int row = rows[r] - 1;
    if (row >= 0 && row < ledRows) {
      int idx = getMatrixIndex(row, col);
      if (idx >= 0 && idx < numLeds) {
        leds[idx] = color;
      }
    }
  }
  FastLED.show();
}

void StartingLights::runCountDownLights(const int* rows, int numRows, unsigned int countDownTime, CRGB color) {
    setAllLightsOff();
    sequenceIsRunning = true;
    mode = MODE_COUNTDOWN;
    countdownNumRows = numRows > 8 ? 8 : numRows;
    memcpy(countdownRows, rows, countdownNumRows * sizeof(int));
    countdownTime = countDownTime;
    countdownColor = color;
    countdownStep = 0;
    countdownLastMillis = millis();
}

void StartingLights::runFlashLights(const int* rows, int numRows, unsigned int interval, CRGB color, int maxFlashCount) {
    setAllLightsOff();
    sequenceIsRunning = true;
    mode = MODE_FLASH;
    flashNumRows = numRows > 8 ? 8 : numRows;
    memcpy(flashRows, rows, flashNumRows * sizeof(int));
    flashInterval = interval;
    flashColor = color;
    flashMaxCount = maxFlashCount;
    flashCount = 0;
    flashLedIsOn = true;
    flashLastMillis = millis();
}

void StartingLights::stopRunningSequence() {
    sequenceIsRunning = false;
    mode = MODE_NONE;
    setAllLightsOff();
}

void StartingLights::updateLeds() {
    if (!sequenceIsRunning) return;

    unsigned long now = millis();

    if (mode == MODE_COUNTDOWN) {
        if (now - countdownLastMillis >= countdownTime / ledsPerRow) {
          if (countdownStep >= ledsPerRow) {
              //setAllLightsOff();
              sequenceIsRunning = false;
              mode = MODE_NONE;
              return;
          }
          for (int r = 0; r < countdownNumRows; r++) {
              int row = countdownRows[r] - 1;
              if (row >= 0 && row < ledRows) {
                  for (int i = 0; i <= countdownStep; i++) {
                      leds[getMatrixIndex(row, i)] = countdownColor;
                  }
              }
          }
          FastLED.show();
          countdownStep++;
          countdownLastMillis = now;
      }
    } else if (mode == MODE_FLASH) {
        if (flashMaxCount > 0 && flashCount >= flashMaxCount) {
            setAllLightsOff();
            sequenceIsRunning = false;
            mode = MODE_NONE;
            return;
        }
        if (now - flashLastMillis >= flashInterval) {
            for (int r = 0; r < flashNumRows; r++) {
              int row = flashRows[r] - 1;
              if (row >= 0 && row < ledRows) {
                for (int i = 0; i < ledsPerRow; i++) {
                  leds[getMatrixIndex(row, i)] = flashLedIsOn ? flashColor : CRGB::Black;
                }
              }
            }
            FastLED.show();
            if (flashLedIsOn) flashCount++;
            flashLedIsOn = !flashLedIsOn;
            flashLastMillis = now;
        }
    }
}
