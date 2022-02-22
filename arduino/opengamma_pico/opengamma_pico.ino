/*
   Open Gamma Detector Sketch
   To be used with the Raspberry Pi Pico!
   
   Triggers on newly detected pulses and prints the
   measured ADC value to the serial port, then resets.

   2022, NuclearPhoenix.
*/

#include <PicoAnalogCorrection.h> // Analog Calibration

const uint8_t GND_PIN = A0; // GND meas pin
const uint8_t VCC_PIN = A2; // VCC meas pin
const uint8_t VSYS_MEAS = A3; // VSYS/3
const uint8_t VBUS_MEAS = 24; // VBUS Sense Pin

const uint8_t AIN_PIN = A1; // Analog input pin
const uint8_t INT_PIN = 13; // Signal interrupt pin
const uint8_t RST_PIN = 5; // Peak detector MOSFET reset pin
const uint8_t LED = 25; // LED on GP25

PicoAnalogCorrection pico; // (2,4095)

void event_int() {
  digitalWrite(LED, HIGH); // Activity LED

  uint16_t m = pico.analogCRead(AIN_PIN,5); // Average 5 corrected measurements
  
  digitalWrite(RST_PIN, HIGH); // Reset peak detector
  delayMicroseconds(5);
  digitalWrite(RST_PIN, LOW);
  
  Serial.println(m);
  
  digitalWrite(LED, LOW);
}

void setup() {
  pinMode(PS_PIN, OUTPUT);
  digitalWrite(PS_PIN, LOW); // Enable Power-Saving
  
  pinMode(GND_PIN, INPUT);
  pinMode(VCC_PIN, INPUT);
  pinMode(VSYS_MEAS, INPUT);
  pinMode(VBUS_MEAS, INPUT);
}

void setup1() {
  pinMode(INT_PIN, INPUT);
  pinMode(RST_PIN, OUTPUT);
  pinMode(AIN_PIN, INPUT);
  pinMode(LED, OUTPUT);

  analogReadResolution(12); // 12-bit ADC, 4096 channels

  attachInterrupt(digitalPinToInterrupt(INT_PIN), event_int, HIGH);

  Serial.begin();
  while(!Serial) {
    ; // Wait for Serial
  }

  //pico.calibrateAdc(GND_PIN, VCC_PIN, 5000);
  //Serial.print("Offset: ");
  //pico.returnCalibrationValues();
}

void loop() {
  /*
  digitalWrite(PS_PIN, HIGH); // Disable Power Save For Better Noise
  Serial.print("Temperature: ");
  Serial.println(analogReadTemp(),1);
  digitalWrite(PS_PIN, LOW); // Re-Enable Power Saving
  
  Serial.print("Supply Voltage: ");
  Serial.println(3*pico.analogCRead(VSYS_MEAS)*3.3/4095,3);
  
  Serial.print("USB Connection: ");
  Serial.println(digitalRead(VBUS_MEAS));
  Serial.println();
  */
  delay(1000);
}

void loop1() {
  // Do not use, because interrupts run on this core!
}
