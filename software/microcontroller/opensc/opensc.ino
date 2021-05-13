/*
   Scintillation Counter Sketch
   Simple detector sketch returning the pulse heights
*/

#include <SAMD_AnalogCorrection.h>
#include <Adafruit_DotStar.h>

Adafruit_DotStar led(1, 8, 6, DOTSTAR_RGB);

const uint8_t AIN_PIN = A5; // Analog input pin
const uint8_t INT_PIN = 10; // Signal interrupt pin
const uint8_t RST_PIN = 9; // Peak detector MOSFET reset pin

void set_lights(const uint8_t r = 0, const uint8_t g = 0, const uint8_t b = 0) {
  //led.clear();
  led.setPixelColor(0, r, g, b);
  led.show();
}

void event_int() {
  digitalWrite(LED_BUILTIN, HIGH); // Activity LED

  uint16_t m1 = analogRead(AIN_PIN); // Average 3 measurements
  uint16_t m2 = analogRead(AIN_PIN);
  uint16_t m3 = analogRead(AIN_PIN);
  Serial.println(3.3 / 4095.0 * (m1 + m2 + m3) / 3.0, 3);
  
  digitalWrite(RST_PIN, HIGH); // Reset peak detector
  delayMicroseconds(1);
  digitalWrite(RST_PIN, LOW);
  
  digitalWrite(LED_BUILTIN, LOW);
}

void setup() {
  pinMode(INT_PIN, INPUT);
  pinMode(RST_PIN, OUTPUT);
  pinMode(AIN_PIN, INPUT);
  pinMode(LED_BUILTIN, OUTPUT);

  // LED STARTUP
  led.begin(); // Init onboard DotStar LED
  led.setBrightness(5); // Turn down brightness
  led.show(); // Initialize all pixels to "off"

  analogReference(AR_DEFAULT);
  analogReadResolution(12); // 12-bit ADC readouts
  analogReadCorrection(11, 2054); // ADC Calibration

  attachInterrupt(digitalPinToInterrupt(INT_PIN), event_int, HIGH);

  set_lights(0, 255, 0);

  Serial.begin(2000000);
}

void loop() {
}
