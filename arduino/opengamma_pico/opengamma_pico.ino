/*
   Open Gamma Detector Sketch
   To be used with the Raspberry Pi Pico!

   Triggers on newly detected pulses and prints the
   measured ADC value to the serial port, then resets.

   2022, NuclearPhoenix.

   TODO: Enable file system writing for saving spectra.
*/

#include <PicoAnalogCorrection.h> // Analog Calibration
#include <SimpleShell.h> // Serial Commands
//#include <LittleFS.h> // On-board Flash File System

const String FWVERS = "1.0.1"; // Firmware Version Code

const uint8_t GND_PIN = A0; // GND meas pin
const uint8_t VCC_PIN = A2; // VCC meas pin
const uint8_t VSYS_MEAS = A3; // VSYS/3
const uint8_t VBUS_MEAS = 24; // VBUS Sense Pin

const uint8_t AIN_PIN = A1; // Analog input pin
const uint8_t INT_PIN = 13; // Signal interrupt pin
const uint8_t RST_PIN = 5; // Peak detector MOSFET reset pin
const uint8_t LED = 25; // LED on GP25

const uint8_t ADC_RES = 12; // Use 12-bit ADC resolution

PicoAnalogCorrection pico(ADC_RES); // (10,4092)

//File f;

volatile bool ser_output = true; // Wheter data should be Serial.println'ed
//volatile bool flash_save = false; // Wheter data will be written to flash
volatile bool geiger_mode = false; // Measure only cps, not energy

volatile uint32_t spectrum[4096]; // Holds the spectrum histogram written to flash
String output_data; // Serial output string of all events 


void toggleSerialInterrupt(String *args) {
  String command = *args;
  command.replace("ser int -","");
  command.trim();

  if (command == "enable") {
    ser_output = true;
  } else if (command == "disable") {
    ser_output = false;
  } else {
    Serial.println("No valid input '" + command + "'!");
  }
}


void toggleGeigerMode(String *args) {
  String command = *args;
  command.replace("set mode -","");
  command.trim();

  if (command == "geiger") {
    geiger_mode = true;
  } else if (command == "energy") {
    geiger_mode = false;
  } else {
    Serial.println("No valid input '" + command + "'!");
  }
}

/*
void toggleFileWrite(String *args) {
  String command = *args;
  command.replace("fs write -","");
  command.trim();

  if (command == "enable") {
    flash_save = true;
  } else if (command == "disable") {
    flash_save = false;
  } else {
    Serial.println("No valid input '" + command + "'!");
  }
}
*/

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
  Serial.print(3.0 * pico.analogCRead(VSYS_MEAS,5) * 3.3 / 4095.0, 3);
  Serial.println(" V");
}


void readCalibration(String *args) {
  Serial.print("Offset: ");
  pico.returnCalibrationValues();
}


void setCalibration(String *args) {
  String command = *args;
  command.replace("cal calibrate -","");
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

/*
void filesysInfo(String *args) {
  FSInfo fs_info;
  Serial.println(LittleFS.info(fs_info));
}
*/

void deviceInfo(String *args) {
  Serial.println("Open Gamma Counter");
  Serial.println("2022. github.com/Open-Gamma-Project");
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

  uint16_t mean;

  if (geiger_mode) {
    mean = 1;
  } else {
    mean = pico.analogCRead(AIN_PIN, 5);
  }
  

  /*
  uint8_t msize = 10;
  uint16_t meas[msize];

  for (size_t i = 0; i < msize; i++) {
    meas[i] = pico.analogCRead(AIN_PIN);;
  }

  float mean = 0.0;
  for (size_t i = 0; i < msize; i++) {
  mean += meas[i];
  }
  mean /= msize;

  float var = 0.0;
  for (size_t i = 0; i < msize; i++) {
  var += sq(meas[i] - mean);
  }
  var /= msize;
  */

  digitalWrite(RST_PIN, HIGH); // Reset peak detector
  delayMicroseconds(1); // Discharge for 1µs, plenty for over 99% discharge
  digitalWrite(RST_PIN, LOW);

  /*
  if (ser_output) {
    if (Serial) {
      Serial.print(String(mean) + ';');
      //Serial.print(' ' + String(sqrt(var)) + ';');
      //Serial.println(' ' + String(sqrt(var)/mean) + ';');
    }
  }
  */

  /*
  if (flash_save) {
    spectrum[mean] += 1;
  }
  */

  if (ser_output) {
    output_data += String(mean) + ";";
  }
  spectrum[mean] += 1;

  digitalWrite(LED, LOW);
}


void setup() {
  pinMode(PS_PIN, OUTPUT);
  digitalWrite(PS_PIN, LOW); // Enable Power-Saving

  pinMode(GND_PIN, INPUT);
  pinMode(VCC_PIN, INPUT);
  pinMode(VSYS_MEAS, INPUT);
  pinMode(VBUS_MEAS, INPUT);

  //LittleFS.begin(); // Start LittleFS, will autoformat if nothing can be mounted
  //f = LittleFS.open("/example.txt", "w"); // Open a test file

  Shell.registerCommand(new ShellCommand(readTemp, F("read temp")));
  Shell.registerCommand(new ShellCommand(readSupplyVoltage, F("read vsys")));
  Shell.registerCommand(new ShellCommand(readUSB, F("read usb")));
  Shell.registerCommand(new ShellCommand(readCalibration, F("read cal")));

  Shell.registerCommand(new ShellCommand(getSpectrumData, F("read spectrum")));

  Shell.registerCommand(new ShellCommand(toggleGeigerMode, F("set mode -")));
  Shell.registerCommand(new ShellCommand(setCalibration, F("cal calibrate -")));
  Shell.registerCommand(new ShellCommand(toggleSerialInterrupt, F("ser int -")));
  //Shell.registerCommand(new ShellCommand(toggleFileWrite, F("fs write -")));
  //Shell.registerCommand(new ShellCommand(filesysInfo, F("fs info")));
  Shell.registerCommand(new ShellCommand(deviceInfo, F("ogc info")));

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


void setup1() {
  pinMode(INT_PIN, INPUT);
  pinMode(RST_PIN, OUTPUT);
  pinMode(AIN_PIN, INPUT);
  pinMode(LED, OUTPUT);

  analogReadResolution(ADC_RES);

  attachInterrupt(digitalPinToInterrupt(INT_PIN), eventInt, HIGH);
}


void loop() {
  /*
  if (flash_save) { // Save data to flash file system
    unsigned long micro = micros();
    if (f) {
      for (size_t i = 0; i < 4096; i++) {
        f.println(spectrum[i]);
      }
    }
    Serial.println(micros() - micro);
  }
  */

  if (ser_output) {
    if (Serial) {
      Serial.println(output_data);
    }
    output_data = "";
  }
  
  delay(1000); // Wait for 1 sec
}


void loop1() {
  // Do not use, because interrupts run on this core!
}
