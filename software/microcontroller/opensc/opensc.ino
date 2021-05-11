/*
   Scintillation Counter Sketch
   Simple detector sketch returning the pulse heights
*/

#include <SAMD_AnalogCorrection.h>
#include <Adafruit_DotStar.h>

Adafruit_DotStar strip(1, 8, 6, DOTSTAR_RGB);

const uint8_t AIN_PIN = A5; // Analog input pin
const uint8_t INT_PIN = 10; // Signal interrupt pin
const uint8_t RST_PIN = 9; // Peak detector MOSFET reset pin

void set_lights(const uint8_t r = 0, const uint8_t g = 0, const uint8_t b = 0) {
  //strip.clear();
  strip.setPixelColor(0, r, g, b);
  strip.show();
}

void event_int() {
  digitalWrite(LED_BUILTIN, HIGH); // Activity LED
  digitalWrite(RST_PIN, HIGH); // Reset peak detector

  Serial.println(3.3 / 4095 * analogRead(AIN_PIN), 3);

  digitalWrite(RST_PIN, LOW);
  digitalWrite(LED_BUILTIN, LOW);
}

void setup() {
  pinMode(INT_PIN, INPUT);
  pinMode(RST_PIN, OUTPUT);
  pinMode(AIN_PIN, INPUT);
  pinMode(LED_BUILTIN, OUTPUT);

  // LED STARTUP
  strip.begin(); // Init onboard DotStar LED
  strip.setBrightness(5); // Turn down brightness
  strip.show(); // Initialize all pixels to "off"

  analogReference(AR_DEFAULT);
  analogReadResolution(12); // 12-bit ADC readouts
  analogReadCorrection(11, 2054); // ADC Calibration

  attachInterrupt(digitalPinToInterrupt(INT_PIN), event_int, HIGH);

  set_lights(0, 255, 0);

  Serial.begin(2000000);
}

void loop() {
}
