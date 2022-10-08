/*

   Open Gamma Detector Sketch
   Only works on the Raspberry Pi Pico with arduino-pico!

   Triggers on newly detected pulses and measures their energy.

   2022, NuclearPhoenix. Open Gamma Project.
   https://github.com/Open-Gamma-Project/Open-Gamma-Detector

   ## NOTE:
   ## Only change the highlighted USER SETTINGS
   ## except you know exactly what you are doing!
   ## Flash with default settings and
   ##   Flash Size: 2MB (Sketch: 1984KB, FS: 64KB)

   TODO: There are still some ADC issues?
   TODO: Coincidence measurements?

*/

#include <hardware/adc.h>  // For corrected temp readings

#include <SimpleShell.h>  // Serial Commands/Console
#include <ArduinoJson.h>  // Load and save the settings file
#include <LittleFS.h>     // Used for FS, stores the settings file

const String FWVERS = "2.2.2";  // Firmware Version Code

const uint8_t GND_PIN = A0;    // GND meas pin
const uint8_t VCC_PIN = A2;    // VCC meas pin
const uint8_t VSYS_MEAS = A3;  // VSYS/3
const uint8_t VBUS_MEAS = 24;  // VBUS Sense Pin
const uint8_t PS_PIN = 23;     // SMPS power save pin

const uint8_t AIN_PIN = A1;  // Analog input pin
const uint8_t INT_PIN = 16;  // Signal interrupt pin
const uint8_t RST_PIN = 22;  // Peak detector MOSFET reset pin
const uint8_t LED = 25;      // LED on GP25

/*
    BEGIN USER SETTINGS
*/
// These are the default settings that can only be changed by reflashing the Pico
const float VREF_VOLTAGE = 3.3;       // ADC reference voltage, defaults 3.3, with reference 3.0
const uint8_t ADC_RES = 12;           // Use 12-bit ADC resolution
const uint16_t EVENT_BUFFER = 50000;  // Buffer this many events for Serial.print

struct Config {
  // These are the default settings that can also be changes via the serial commands
  volatile bool ser_output = true;       // Wheter data should be Serial.println'ed
  volatile bool geiger_mode = false;     // Measure only cps, not energy
  volatile bool print_spectrum = false;  // Print the finishes spectrum, not just
  volatile bool auto_reset = true;       // Periodically reset S&H circuit
  volatile uint8_t meas_avg = 5;         // Number of meas. averaged each event, higher=longer dead time
};
/*
   END USER SETTINGS
*/

volatile uint32_t spectrum[uint16_t(pow(2, ADC_RES))];  // Holds the spectrum histogram written to flash
volatile uint16_t events[EVENT_BUFFER];                 // Buffer array for single events
volatile uint16_t event_position = 0;                   // Target index in events array

volatile Config conf;  // Configuration object


void serialInterruptMode(String *args) {
  String command = *args;
  command.replace("set int -", "");
  command.trim();

  if (command == "spectrum") {
    conf.ser_output = true;
    conf.print_spectrum = true;
    Serial.println("Set serial interrupt mode to full spectrum.");
  } else if (command == "events") {
    conf.ser_output = true;
    conf.print_spectrum = false;
    Serial.println("Set serial interrupt mode to events.");
  } else if (command == "disable") {
    conf.ser_output = false;
    conf.print_spectrum = false;
    Serial.println("Disabled serial interrupts.");
  } else {
    Serial.println("No valid input '" + command + "'!");
    Serial.println("Must be 'spectrum', 'events' or 'disable'.");
    return;
  }
  saveSetting();
}


void toggleAutoReset(String *args) {
  String command = *args;
  command.replace("set reset -", "");
  command.trim();

  if (command == "enable") {
    conf.auto_reset = true;
    Serial.println("Enabled auto-reset.");
  } else if (command == "disable") {
    conf.auto_reset = false;
    Serial.println("Disabled auto-reset.");
  } else {
    Serial.println("No valid input '" + command + "'!");
    Serial.println("Must be 'enable' or 'disable'.");
    return;
  }
  saveSetting();
}


void toggleGeigerMode(String *args) {
  String command = *args;
  command.replace("set mode -", "");
  command.trim();

  if (command == "geiger") {
    conf.geiger_mode = true;
    event_position = 0;
    Serial.println("Enabled geiger mode.");
  } else if (command == "energy") {
    conf.geiger_mode = false;
    event_position = 0;
    Serial.println("Enabled energy measuring mode.");
  } else {
    Serial.println("No valid input '" + command + "'!");
    Serial.println("Must be 'geiger' or 'energy'.");
    return;
  }
  saveSetting();
}


void measAveraging(String *args) {
  String command = *args;
  command.replace("set averaging -", "");
  command.trim();
  const uint8_t number = (uint8_t)command.toInt();

  if (number > 0) {
    conf.meas_avg = number;
    Serial.println("Set measurement averaging to " + String(number) + ".");
  } else {
    Serial.println("No valid input '" + command + "'!");
    Serial.println("Parameter must be a number >= 0.");
    return;
  }
  saveSetting();
}


float analogReadTempCorrect() {
  // Copy from arduino-pico/cores/rp2040/wiring_analog.cpp
  adc_init();  // Init ADC just to be sure
  adc_set_temp_sensor_enabled(true);

  // ## ADDED: Disable interrupts shortly
  detachInterrupt(digitalPinToInterrupt(INT_PIN));
  //digitalWrite(PS_PIN, HIGH); // Disable Power Save For Better Noise

  delay(1);  // Allow things to settle.  Without this, readings can be erratic
  adc_select_input(4);
  int v = adc_read();

  // ## ADDED: Reset S&H after the detached interrupt and re-enable interrupts
  //digitalWrite(PS_PIN, LOW); // Re-Enable Power Saving
  resetSampleHold();
  attachInterrupt(digitalPinToInterrupt(INT_PIN), eventInt, HIGH);

  adc_set_temp_sensor_enabled(false);
  return 27.0 - ((v * VREF_VOLTAGE / (pow(2, ADC_RES) - 1)) - 0.706) / 0.001721;
}


void readTemp(String *args) {
  Serial.print("Temperature: ");
  Serial.print(analogReadTempCorrect(), 1);
  Serial.println(" Celsius");
  //Serial.println(*args); // Print the arguments we've got
}


void readUSB(String *args) {
  Serial.print("USB Connection: ");
  Serial.println(digitalRead(VBUS_MEAS));
}


void readSupplyVoltage(String *args) {
  Serial.print("Supply Voltage: ");
  Serial.print(3.0 * analogRead(VSYS_MEAS) * VREF_VOLTAGE / (pow(2, ADC_RES) - 1), 3);
  Serial.println(" V");
}


void deviceInfo(String *args) {
  Serial.println("=========================");
  Serial.println("-- Open Gamma Detector --");
  Serial.println("By NuclearPhoenix, Open Gamma Project");
  Serial.println("2022. https://github.com/Open-Gamma-Project");
  Serial.println("Firmware Version: " + FWVERS);
  Serial.println("=========================");
  Serial.println("CPU Frequency: " + String(rp2040.f_cpu() / 1e6) + " MHz");
  Serial.println("Runtime: " + String(millis() / 1000.0) + " s");
  Serial.println("Free Heap Memory: " + String(rp2040.getFreeHeap()) + " B");
  Serial.println("Used Heap Memory: " + String(rp2040.getUsedHeap()) + " B");
  Serial.println("Total Heap Size: " + String(rp2040.getTotalHeap()) + " B");
}


void fsInfo(String *args) {
  FSInfo64 fsinfo;
  LittleFS.info64(fsinfo);
  Serial.print("Total Bytes: ");
  Serial.println(fsinfo.totalBytes);
  Serial.print("Used Bytes: ");
  Serial.println(fsinfo.usedBytes);
  Serial.print("Block Size: ");
  Serial.println(fsinfo.blockSize);
  Serial.print("Page Size: ");
  Serial.println(fsinfo.pageSize);
  Serial.print("Max Open Files: ");
  Serial.println(fsinfo.maxOpenFiles);
  Serial.print("Max Path Length: ");
  Serial.println(fsinfo.maxPathLength);
}


void getSpectrumData(String *args) {
  for (size_t i = 0; i < pow(2, ADC_RES); i++) {
    Serial.println(spectrum[i]);
  }
}


void clearSpectrumData(String *args) {
  Serial.println("Clearing spectrum...");
  for (size_t i = 0; i < pow(2, ADC_RES); i++) {
    spectrum[i] = 0;
  }
  Serial.println("Cleared!");
}


void resetSettings(String *args) {
  const Config defaultConf;
  conf.ser_output = defaultConf.ser_output;
  conf.geiger_mode = defaultConf.geiger_mode;
  conf.print_spectrum = defaultConf.print_spectrum;
  conf.auto_reset = defaultConf.auto_reset;
  conf.meas_avg = defaultConf.meas_avg;
  Serial.println("Applied default settings.");
  saveSetting();
}


void rebootNow(String *args) {
  Serial.println("You might need to re-connect after reboot.");
  Serial.println("Rebooting now...");
  delay(1000);
  rp2040.reboot();
}


void serialEvent() {
  Shell.handleEvent();  // Handle the serial input
}


void saveSetting() {
  // Physically save file everytime a setting changes.
  // Not the best for flash longevity, but it's ok.
  File saveFile = LittleFS.open("/config.json", "w");

  if (!saveFile) {
    Serial.println("Could not open save file!");
    return;
  }

  const int capacity = JSON_ARRAY_SIZE(5);  // Number of settings that will be saved
  //DynamicJsonDocument doc(72);
  StaticJsonDocument<capacity> doc;

  doc["ser_output"] = conf.ser_output;
  doc["geiger_mode"] = conf.geiger_mode;
  doc["print_spectrum"] = conf.print_spectrum;
  doc["auto_reset"] = conf.auto_reset;
  doc["meas_avg"] = conf.meas_avg;

  serializeJson(doc, saveFile);  //serializeJsonPretty()

  saveFile.close();
  Serial.println("Successfuly written config to flash.");
}


void readSetting() {
  File saveFile = LittleFS.open("/config.json", "r");

  if (!saveFile) {
    Serial.println("Could not open save file!");
    return;
  }

  String json = "";

  while (saveFile.available()) {
    json += saveFile.readString();
  }

  saveFile.close();

  DynamicJsonDocument doc(1024);
  DeserializationError error = deserializeJson(doc, json);

  if (error) {
    Serial.print("Could not load config from json file: ");
    Serial.println(error.f_str());
    return;
  }

  conf.ser_output = doc["ser_output"];
  conf.geiger_mode = doc["geiger_mode"];
  conf.print_spectrum = doc["print_spectrum"];
  conf.auto_reset = doc["auto_reset"];
  conf.meas_avg = doc["meas_avg"];
  Serial.println("Successfuly loaded config from flash.");
}


void resetSampleHold() {  // Reset sample and hold circuit
  digitalWrite(RST_PIN, HIGH);
  delayMicroseconds(1);  // Discharge for 1 µs, actually takes 2 µs - enough for a discharge
  digitalWrite(RST_PIN, LOW);
}


void eventInt() {
  digitalWrite(LED, HIGH);  // Activity LED

  uint16_t mean = 0;
  delayMicroseconds(1);  // Wait to allow the sample/hold circuit to stabilize

  if (!conf.geiger_mode) {
    uint16_t meas[conf.meas_avg];

    //digitalWrite(PS_PIN, HIGH); // Disable Power-Saving
    for (size_t i = 0; i < conf.meas_avg; i++) {
      meas[i] = analogRead(AIN_PIN);
    }
    //digitalWrite(PS_PIN, LOW); // Enable Power-Saving

    float avg = 0.0;
    uint8_t invalid = 0;
    for (size_t i = 0; i < conf.meas_avg; i++) {
      // Pico-ADC DNL issues, see https://pico-adc.markomo.me/INL-DNL/#dnl
      // Discard channels 512, 1536, 2560, and 3584. For now.
      // See RP2040 datasheet Appendix B: Errata
      if (meas[i] == 511 || meas[i] == 1535 || meas[i] == 2559 || meas[i] == 3583) {
        invalid++;
        continue;  // Discard
      }
      avg += meas[i];
    }

    if (conf.meas_avg - invalid <= 0) {  // Catch divide by zero crash
      avg = 0;
    } else {
      avg /= (conf.meas_avg - invalid);
    }

    mean = round(avg);  // float --> uint16_t ADC channel
    // Use median instead of average?

    /*
      float var = 0.0;
      for (size_t i = 0; i < conf.meas_avg; i++) {
      var += sq(meas[i] - mean);
      }
      var /= conf.meas_avg;
    */

    if (conf.ser_output) {
      events[event_position] = mean;
      spectrum[mean] += 1;
      //Serial.print(' ' + String(sqrt(var)) + ';');
      //Serial.println(' ' + String(sqrt(var)/mean) + ';');
      if (event_position < EVENT_BUFFER - 1) {  // Only increment if
        event_position++;
      }
    }
  }

  resetSampleHold();

  digitalWrite(LED, LOW);
}

/*
    SETUP FUNCTIONS
*/
void setup() {
  rp2040.wdt_begin(5000);  // Enable hardware watchdog to check every 5s

  pinMode(INT_PIN, INPUT);
  pinMode(RST_PIN, OUTPUT_12MA);
  pinMode(AIN_PIN, INPUT);
  pinMode(LED, OUTPUT);

  analogReadResolution(ADC_RES);

  resetSampleHold();  // Reset before enabling the interrupts to avoid jamming

  attachInterrupt(digitalPinToInterrupt(INT_PIN), eventInt, HIGH);
}


void setup1() {
  pinMode(PS_PIN, OUTPUT_4MA);
  digitalWrite(PS_PIN, LOW);  // Enable Power-Saving

  pinMode(GND_PIN, INPUT);
  pinMode(VCC_PIN, INPUT);
  pinMode(VSYS_MEAS, INPUT);
  pinMode(VBUS_MEAS, INPUT);

  Shell.registerCommand(new ShellCommand(readTemp, F("read temp")));
  Shell.registerCommand(new ShellCommand(readSupplyVoltage, F("read vsys")));
  Shell.registerCommand(new ShellCommand(readUSB, F("read usb")));
  Shell.registerCommand(new ShellCommand(getSpectrumData, F("read spectrum")));
  Shell.registerCommand(new ShellCommand(deviceInfo, F("read info")));
  Shell.registerCommand(new ShellCommand(fsInfo, F("read fs")));

  Shell.registerCommand(new ShellCommand(toggleGeigerMode, F("set mode -")));
  Shell.registerCommand(new ShellCommand(serialInterruptMode, F("set int -")));
  Shell.registerCommand(new ShellCommand(toggleAutoReset, F("set reset -")));
  Shell.registerCommand(new ShellCommand(measAveraging, F("set averaging -")));

  Shell.registerCommand(new ShellCommand(clearSpectrumData, F("clear spectrum")));
  Shell.registerCommand(new ShellCommand(resetSettings, F("reset settings")));
  Shell.registerCommand(new ShellCommand(rebootNow, F("reboot")));

  Shell.begin(2000000);

  /*
    LittleFSConfig cfg;
    cfg.setAutoFormat(false);
    LittleFS.setConfig(cfg);
  */
  LittleFS.begin();  // Starts FileSystem, autoformats if no FS is detected

  readSetting();  // Read all the detector settings from flash
  /*
    Serial.begin();
    while(!Serial) {
    ; // Wait for Serial
    }
  */

  Serial.println("Welcome to the Open Gamma Detector!");
  Serial.println("Firmware Version " + FWVERS);
}

/*
   LOOP FUNCTIONS
*/
void loop() {
  // Interrupts run on this core
  //__wfi(); // Wait For Interrupt

  if (conf.auto_reset) {
    resetSampleHold();
  }

  delayMicroseconds(500);
}


void loop1() {
  if (conf.ser_output) {
    if (Serial) {
      if (conf.print_spectrum) {
        for (uint16_t index = 0; index < uint16_t(pow(2, ADC_RES)); index++) {
          Serial.print(String(spectrum[index]) + ";");
          //spectrum[index] = 0;
        }
        Serial.println();
      } else if (event_position > 0) {
        for (uint16_t index = 0; index < event_position; index++) {
          Serial.print(String(events[index]) + ";");
        }
        Serial.println();
      }
    }

    event_position = 0;
  }

  rp2040.wdt_reset();  // Reset watchdog, everything is fine

  delay(1000);  // Wait for 1 sec, better: sleep for power saving?!
}