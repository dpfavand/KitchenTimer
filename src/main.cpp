#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

#define DISPLAY_ADDRESS 0x3C // alternative: 0x78 0x7A
#define BUTTON_PIN D5 //D5

Adafruit_SSD1306 display(128, 64, &Wire);

void setup() {
  Serial.begin(9600);
  Serial.println("");
  Serial.println("setup()");
  delay(500);

  // On D1 mini: SDA = D2 (GPIO4), SCL = D1 (GPIO5).
  Wire.begin(D2, D1);
  Serial.println("now display start");

  if (!display.begin(SSD1306_SWITCHCAPVCC, DISPLAY_ADDRESS)) {
    Serial.println("SSD1306 init failed");
    for (;;) {
      delay(1000);
    }
  }

  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 20);
  display.println("Hello,");
  display.println("World!");
  display.display();
  Serial.println("display start done");
}

void loop() {
  // Nothing else to do.
}