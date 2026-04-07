#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

#define DISPLAY_ADDRESS 0x3C // alternative: 0x78 0x7A
#define BUTTON_PIN D5 //D5

#define MODE_SELECT_TIMER 0
#define MODE_TIMER_RUNNING 1

const unsigned long DEBOUNCE_MS = 40;
const unsigned long LONG_PRESS_MS = 800;

Adafruit_SSD1306 display(128, 64, &Wire);

bool lastRawButtonState = HIGH;
bool stableButtonState = HIGH;
unsigned long lastDebounceTime = 0;
unsigned long pressStartTime = 0;
bool pressActive = false;

void showMessage(const char *line1, const char *line2 = "") {
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 20);
  display.println(line1);
  if (line2[0] != '\0') {
    display.println(line2);
  }
  display.display();
}

void handleShortPress() {
  Serial.println("Short press");
  showMessage("Short", "Press");
}

void handleLongPress() {
  Serial.println("Long press");
  showMessage("Long", "Press");
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

  showMessage("Hello,", "World!");
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
    } else if (pressActive) {
      unsigned long pressDuration = millis() - pressStartTime;
      if (pressDuration >= LONG_PRESS_MS) {
        handleLongPress();
      } else {
        handleShortPress();
      }
      pressActive = false;
    }
  }
}