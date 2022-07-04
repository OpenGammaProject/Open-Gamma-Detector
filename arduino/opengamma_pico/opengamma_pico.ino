/*
   Open Gamma Detector Sketch
   To be used with the Raspberry Pi Pico!

   Triggers on newly detected pulses and prints the
   measured ADC value to the serial port, then resets.

   2022, NuclearPhoenix.

   TODO: Enable file system writing for saving spectra.
   TODO: Fix ADC issues with averaging around the channels (3 channels 4x)
   TODO: Save settings in flash storage!
   TODO: Clear Spectrum Function
*/

#include <PicoAnalogCorrection.h> // Analog Calibration
#include <SimpleShell.h> // Serial Commands

const String FWVERS = "1.1.1"; // Firmware Version Code

const uint8_t GND_PIN = A0; // GND meas pin
const uint8_t VCC_PIN = A2; // VCC meas pin
const uint8_t VSYS_MEAS = A3; // VSYS/3
const uint8_t VBUS_MEAS = 24; // VBUS Sense Pin

const uint8_t AIN_PIN = A1; // Analog input pin
const uint8_t INT_PIN = 13; // Signal interrupt pin
const uint8_t RST_PIN = 5; // Peak detector MOSFET reset pin
const uint8_t LED = 25; // LED on GP25

const uint8_t ADC_RES = 12; // Use 12-bit ADC resolution
const uint16_t EVENT_SPACE = 50000; // Buffer this many events for Serial.print

PicoAnalogCorrection pico(ADC_RES); // (10,4092)

volatile bool ser_output = true; // Wheter data should be Serial.println'ed
volatile bool geiger_mode = false; // Measure only cps, not energy
volatile bool print_spectrum = false; // Print the finishes spectrum, not just

volatile uint32_t spectrum[uint16_t(pow(2, ADC_RES))]; // Holds the spectrum histogram written to flash
volatile uint16_t events[EVENT_SPACE];
volatile uint16_t event_position = 0;


void serialInterruptMode(String *args) {
  String command = *args;
  command.replace("ser int -", "");
  command.trim();

  if (command == "spectrum") {
    ser_output = true;
    print_spectrum = true;
  } else if (command == "events") {
    ser_output = true;
    print_spectrum = false;
  } else if (command == "disable") {
    ser_output = false;
  } else {
    Serial.println("No valid input '" + command + "'!");
  }
}


void toggleGeigerMode(String *args) {
  String command = *args;
  command.replace("set mode -", "");
  command.trim();

  if (command == "geiger") {
    geiger_mode = true;
    event_position = 0;
  } else if (command == "energy") {
    geiger_mode = false;
    event_position = 0;
  } else {
    Serial.println("No valid input '" + command + "'!");
  }
}


void readTemp(String *args) {
  Serial.print("Temperature: ");

  digitalWrite(PS_PIN, HIGH); // Disable Power Save For Better Noise
  Serial.print(analogReadTemp(), 2);
  digitalWrite(PS_PIN, LOW); // Re-Enable Power Saving

  Serial.println(" Celsius");

  //Serial.println(*args); // Print the arguments we've got
}


void readUSB(String *args) {
  Serial.print("USB Connection: ");
  Serial.println(digitalRead(VBUS_MEAS));
}


void readSupplyVoltage(String *args) {
  Serial.print("Supply Voltage: ");
  Serial.print(3.0 * pico.analogCRead(VSYS_MEAS, 5) * 3.3 / 4095.0, 3);
  Serial.println(" V");
}


void readCalibration(String *args) {
  Serial.print("Offset: ");
  pico.returnCalibrationValues();
}


void setCalibration(String *args) {
  String command = *args;
  command.replace("cal calibrate -", "");
  command.trim();

  long meas_num = command.toInt();

  if (meas_num <= 0) {
    Serial.println("No valid input '" + command + "'!");
    return;
  }

  Serial.println("Calibrating using " + String(meas_num) + " ADC measurements.");
  Serial.println("Please wait.");

  pico.calibrateAdc(GND_PIN, VCC_PIN, meas_num);

  Serial.println("Done calibrating.");

  readCalibration(args);
}


void deviceInfo(String *args) {
  Serial.println("--Open Gamma Detector--");
  Serial.println("By NuclearPhoenix, Open Gamma Project");
  Serial.println("2022. https://github.com/Open-Gamma-Project");
  Serial.println("Firmware Version: " + FWVERS);
}


void getSpectrumData(String *args) {
  for (size_t i = 0; i < 4096; i++) {
    Serial.println(spectrum[i]);
  }
}


void serialEvent() {
  Shell.handleEvent(); // Handle the serial input
}


void eventInt() {
  digitalWrite(LED, HIGH); // Activity LED

  uint16_t mean = 0;
  delayMicroseconds(1); // Wait to allow the sample/hold circuit to stabilize

  if (!geiger_mode) {
    //mean = pico.analogCRead(AIN_PIN, 5);

    uint8_t msize = 5; // x measurements to average
    uint16_t meas[msize];

    //digitalWrite(PS_PIN, HIGH); // Disable Power-Saving
    for (size_t i = 0; i < msize; i++) {
      meas[i] = analogRead(AIN_PIN);
    }
    //digitalWrite(PS_PIN, LOW); // Enable Power-Saving

    float avg = 0.0;
    uint8_t invalid = 0;
    for (size_t i = 0; i < msize; i++) {
      // Pico-ADC DNL issues, see https://pico-adc.markomo.me/INL-DNL/#dnl
      // Discard channels 512, 1536, 2560, and 3584. For now.
      // See RP2040 datasheet Appendix B: Errata
      if (meas[i] == 511 || meas[i] == 1535 || meas[i] == 2559 || meas[i] == 3583) {
        invalid++;
        continue; // Discard
      }
      avg += meas[i];
    }

    if (msize - invalid <= 0) { // Catch divide by zero crash
      avg = 0;
    } else {
      avg /= (msize - invalid);
    }

    mean = round(avg); // float --> uint16_t ADC channel
  }

  /*
    float var = 0.0;
    for (size_t i = 0; i < msize; i++) {
    var += sq(meas[i] - mean);
    }
    var /= msize;
  */

  if (ser_output) {
    events[event_position] = mean;
    spectrum[mean] += 1;
    //Serial.print(' ' + String(sqrt(var)) + ';');
    //Serial.println(' ' + String(sqrt(var)/mean) + ';');
    if (event_position < EVENT_SPACE-1) { // Only increment if 
      event_position++;
    }
  }

  // Reset sample and hold circuit
  digitalWrite(RST_PIN, HIGH);
  delayMicroseconds(1); // Discharge for 1µs, plenty for over 99% discharge
  digitalWrite(RST_PIN, LOW);

  digitalWrite(LED, LOW);
}


void setup() {
  pinMode(INT_PIN, INPUT);
  pinMode(RST_PIN, OUTPUT_12MA);
  pinMode(AIN_PIN, INPUT);
  pinMode(LED, OUTPUT_4MA);

  analogReadResolution(ADC_RES);

  attachInterrupt(digitalPinToInterrupt(INT_PIN), eventInt, HIGH);
}


void setup1() {
  pinMode(PS_PIN, OUTPUT_4MA);
  digitalWrite(PS_PIN, LOW); // Enable Power-Saving

  pinMode(GND_PIN, INPUT);
  pinMode(VCC_PIN, INPUT);
  pinMode(VSYS_MEAS, INPUT);
  pinMode(VBUS_MEAS, INPUT);

  Shell.registerCommand(new ShellCommand(readTemp, F("read temp")));
  Shell.registerCommand(new ShellCommand(readSupplyVoltage, F("read vsys")));
  Shell.registerCommand(new ShellCommand(readUSB, F("read usb")));
  Shell.registerCommand(new ShellCommand(deviceInfo, F("read info")));
  Shell.registerCommand(new ShellCommand(readCalibration, F("read cal")));
  Shell.registerCommand(new ShellCommand(getSpectrumData, F("read spectrum")));
  Shell.registerCommand(new ShellCommand(toggleGeigerMode, F("set mode -")));
  Shell.registerCommand(new ShellCommand(setCalibration, F("cal calibrate -")));
  Shell.registerCommand(new ShellCommand(serialInterruptMode, F("ser int -")));

  Shell.begin(2000000);

  /*
    Serial.begin();
    while(!Serial) {
    ; // Wait for Serial
    }
  */

  //pico.calibrateAdc(GND_PIN, VCC_PIN, 10000); // Calibrate ADC on start-up

  Serial.println("Welcome from Open Gamma Detector!");
  Serial.println("Firmware Version " + FWVERS);
}


void loop() {
  // Do not use, because interrupts run on this core!
  __wfi(); // Wait For Interrupt
}


void loop1() {
  if (ser_output) {
    if (Serial) {
      if (print_spectrum) {
        for (uint16_t index = 0; index < uint16_t(pow(2, ADC_RES)); index++) {
          Serial.print(String(spectrum[index]) + ";");
          //spectrum[index] = 0;
        }
      } else {
        if (event_position > 0) {
          for (uint16_t index = 0; index < event_position; index++) {
            Serial.print(String(events[index]) + ";");
          }
        }
      }
      Serial.println();
    }

    event_position = 0;
  }

  delay(1000); // Wait for 1 sec, better: sleep for power saving?!
}
