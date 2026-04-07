#include <Arduino.h>
#include <cstdio>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1

#define DISPLAY_ADDRESS 0x3C // alternative: 0x78 0x7A
#define BUTTON_PIN D5 //D5

enum TimerState {
  STATE_SET_INTERVAL_1,
  STATE_SET_INTERVAL_2,
  STATE_RUN_INTERVAL_1,
  STATE_RUN_INTERVAL_2,
  STATE_PAUSED,
};

const unsigned long DEBOUNCE_MS = 40;
const unsigned long LONG_PRESS_MS = 800;
const unsigned long INPUT_LOCKOUT_MS = 200;
const unsigned long COUNTDOWN_TICK_MS = 1000;
const unsigned int EDIT_STEP_SECONDS = 15;
const unsigned int MIN_INTERVAL_SECONDS = 15;
const unsigned int MAX_INTERVAL_SECONDS = 4 * 60;

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

bool lastRawButtonState = HIGH;
bool stableButtonState = HIGH;
unsigned long lastDebounceTime = 0;
unsigned long pressStartTime = 0;
bool pressActive = false;
bool longPressHandled = false;

TimerState currentState = STATE_SET_INTERVAL_1;
TimerState pausedFromState = STATE_RUN_INTERVAL_1;
unsigned int interval1Seconds = 60;
unsigned int interval2Seconds = 15;
unsigned int countdownRemainingSeconds = 60;
unsigned long lastTickTime = 0;
unsigned long lastStateChangeTime = 0;
bool displayDirty = true;

unsigned int clampInterval(unsigned int seconds) {
  return constrain(seconds, MIN_INTERVAL_SECONDS, MAX_INTERVAL_SECONDS);
}

unsigned int advanceIntervalValue(unsigned int currentValue) {
  if (currentValue >= MAX_INTERVAL_SECONDS) {
    return MIN_INTERVAL_SECONDS;
  }

  return clampInterval(currentValue + EDIT_STEP_SECONDS);
}

void formatSeconds(unsigned int totalSeconds, char *buffer, size_t bufferSize) {
  unsigned int minutes = totalSeconds / 60;
  unsigned int seconds = totalSeconds % 60;
  snprintf(buffer, bufferSize, "%02u:%02u", minutes, seconds);
}

void drawCenteredText(const char *line1, const char *line2 = "", uint8_t textSize = 2) {
  display.clearDisplay();
  display.setTextSize(textSize);
  display.setTextColor(SSD1306_WHITE);

  int16_t x1;
  int16_t y1;
  uint16_t width;
  uint16_t height;

  display.getTextBounds(line1, 0, 0, &x1, &y1, &width, &height);
  int16_t line1X = (SCREEN_WIDTH - static_cast<int16_t>(width)) / 2;
  int16_t line1Y = line2[0] == '\0' ? 24 : 10;
  display.setCursor(line1X < 0 ? 0 : line1X, line1Y);
  display.println(line1);

  if (line2[0] != '\0') {
    display.getTextBounds(line2, 0, 0, &x1, &y1, &width, &height);
    int16_t line2X = (SCREEN_WIDTH - static_cast<int16_t>(width)) / 2;
    display.setCursor(line2X < 0 ? 0 : line2X, line1Y + 24);
    display.println(line2);
  }

  display.display();
}

void markDisplayDirty() {
  displayDirty = true;
}

bool isEditState(TimerState state) {
  return state == STATE_SET_INTERVAL_1 || state == STATE_SET_INTERVAL_2;
}

void transitionToState(TimerState nextState) {
  currentState = nextState;
  lastStateChangeTime = millis();
  markDisplayDirty();
}

bool inputLockedOut() {
  return (millis() - lastStateChangeTime) < INPUT_LOCKOUT_MS;
}

void startCountdown(TimerState runState) {
  countdownRemainingSeconds = runState == STATE_RUN_INTERVAL_1 ? interval1Seconds : interval2Seconds;
  lastTickTime = millis();
  transitionToState(runState);
}

void resetToSetup() {
  Serial.println("Reset to interval setup");
  countdownRemainingSeconds = interval1Seconds;
  transitionToState(STATE_SET_INTERVAL_1);
}

void advanceToNextInterval() {
  if (currentState == STATE_RUN_INTERVAL_1) {
    Serial.println("Switching to interval 2");
    startCountdown(STATE_RUN_INTERVAL_2);
  } else {
    Serial.println("Switching to interval 1");
    startCountdown(STATE_RUN_INTERVAL_1);
  }
}

void renderScreen() {
  char timeBuffer[8];

  switch (currentState) {
    case STATE_SET_INTERVAL_1:
      formatSeconds(interval1Seconds, timeBuffer, sizeof(timeBuffer));
      drawCenteredText("Set A", timeBuffer);
      break;

    case STATE_SET_INTERVAL_2:
      formatSeconds(interval2Seconds, timeBuffer, sizeof(timeBuffer));
      drawCenteredText("Set B", timeBuffer);
      break;

    case STATE_RUN_INTERVAL_1:
      formatSeconds(countdownRemainingSeconds, timeBuffer, sizeof(timeBuffer));
      drawCenteredText("Timer A", timeBuffer);
      break;

    case STATE_RUN_INTERVAL_2:
      formatSeconds(countdownRemainingSeconds, timeBuffer, sizeof(timeBuffer));
      drawCenteredText("Timer B", timeBuffer);
      break;

    case STATE_PAUSED:
      formatSeconds(countdownRemainingSeconds, timeBuffer, sizeof(timeBuffer));
      drawCenteredText("Paused", timeBuffer);
      break;
  }

  displayDirty = false;
}

void handleShortPress() {
  if (inputLockedOut()) {
    return;
  }

  switch (currentState) {
    case STATE_SET_INTERVAL_1:
      interval1Seconds = advanceIntervalValue(interval1Seconds);
      Serial.print("Interval A: ");
      Serial.println(interval1Seconds);
      markDisplayDirty();
      break;

    case STATE_SET_INTERVAL_2:
      interval2Seconds = advanceIntervalValue(interval2Seconds);
      Serial.print("Interval B: ");
      Serial.println(interval2Seconds);
      markDisplayDirty();
      break;

    case STATE_RUN_INTERVAL_1:
    case STATE_RUN_INTERVAL_2:
      pausedFromState = currentState;
      Serial.println("Paused countdown");
      transitionToState(STATE_PAUSED);
      break;

    case STATE_PAUSED:
      Serial.println("Resumed countdown");
      lastTickTime = millis();
      transitionToState(pausedFromState);
      break;
  }
}

void handleLongPress() {
  if (inputLockedOut()) {
    return;
  }

  switch (currentState) {
    case STATE_SET_INTERVAL_1:
      Serial.println("Confirmed interval A");
      transitionToState(STATE_SET_INTERVAL_2);
      break;

    case STATE_SET_INTERVAL_2:
      Serial.println("Starting timer loop");
      startCountdown(STATE_RUN_INTERVAL_1);
      break;

    case STATE_RUN_INTERVAL_1:
    case STATE_RUN_INTERVAL_2:
    case STATE_PAUSED:
      resetToSetup();
      break;
  }
}

void setup() {
  Serial.begin(9600);
  Serial.println("");
  Serial.println("setup()");
  delay(500);

  pinMode(BUTTON_PIN, INPUT_PULLUP);

  // On D1 mini: SDA = D2 (GPIO4), SCL = D1 (GPIO5).
  Wire.begin(D2, D1);
  Serial.println("now display start");

  if (!display.begin(SSD1306_SWITCHCAPVCC, DISPLAY_ADDRESS)) {
    Serial.println("SSD1306 init failed");
    for (;;) {
      delay(1000);
    }
  }

  lastTickTime = millis();
  lastStateChangeTime = millis();
  renderScreen();
  Serial.println("display start done");
}

void loop() {
  bool rawButtonState = digitalRead(BUTTON_PIN);

  // Debounce by waiting for input to remain unchanged for DEBOUNCE_MS.
  if (rawButtonState != lastRawButtonState) {
    lastDebounceTime = millis();
    lastRawButtonState = rawButtonState;
  }

  if ((millis() - lastDebounceTime) >= DEBOUNCE_MS && stableButtonState != rawButtonState) {
    stableButtonState = rawButtonState;

    if (stableButtonState == LOW) {
      pressStartTime = millis();
      pressActive = true;
      longPressHandled = false;
    } else if (pressActive) {
      if (!longPressHandled) {
        handleShortPress();
      }

      pressActive = false;
    }
  }

  if (pressActive && !longPressHandled && (millis() - pressStartTime) >= LONG_PRESS_MS) {
    handleLongPress();
    longPressHandled = true;
  }

  if ((currentState == STATE_RUN_INTERVAL_1 || currentState == STATE_RUN_INTERVAL_2) &&
      (millis() - lastTickTime) >= COUNTDOWN_TICK_MS) {
    lastTickTime += COUNTDOWN_TICK_MS;

    if (countdownRemainingSeconds > 0) {
      countdownRemainingSeconds--;
      markDisplayDirty();
    }

    if (countdownRemainingSeconds == 0) {
      advanceToNextInterval();
    }
  }

  if (displayDirty) {
    renderScreen();
  }
}