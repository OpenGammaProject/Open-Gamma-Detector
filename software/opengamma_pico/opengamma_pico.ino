/*

  Open Gamma Detector Sketch
  Only works on the Raspberry Pi Pico with arduino-pico!

  Triggers on newly detected pulses and measures their energy.

  2022, NuclearPhoenix. Open Gamma Project.
  https://github.com/OpenGammaProject/Open-Gamma-Detector

  ## NOTE:
  ## Only change the highlighted USER SETTINGS below
  ## except you know exactly what you are doing!
  ## Flash with default settings and
  ##   Flash Size: 2MB (Sketch: 1984KB, FS: 64KB)

  TODO: (?) Adafruit TinyUSB lib: WebUSB support
  TODO: (?) Optimize for power usage
  TODO: (?) Ticker + PWM Audio
  
  TODO: (!!!) Check P&H auto-reset time

*/

//#include <ADCInput.h> // Special SiPM readout utilizing the ADC FIFO and Round Robin
#include "Helper.h"                // Misc helper functions
#include <SimpleShell_Enhanced.h>  // Serial Commands/Console
#include <ArduinoJson.h>           // Load and save the settings file
#include <LittleFS.h>              // Used for FS, stores the settings file
#include <Adafruit_SSD1306.h>      // Used for OLEDs
//#include <Statistical.h>

const String FWVERS = "3.0.0";  // Firmware Version Code

const uint8_t GND_PIN = A2;    // GND meas pin
const uint8_t VSYS_MEAS = A3;  // VSYS/3
const uint8_t VBUS_MEAS = 24;  // VBUS Sense Pin
const uint8_t PS_PIN = 23;     // SMPS power save pin

const uint8_t AIN_PIN = A1;         // Analog input pin
const uint8_t AMP_PIN = A0;         // Preamp (baseline) meas pin
const uint8_t INT_PIN = 16;         // Signal interrupt pin
const uint8_t RST_PIN = 22;         // Peak detector MOSFET reset pin
const uint8_t LED = 25;             // LED on GP25
const uint16_t EVT_RESET_C = 2000;  // Number of counts after which the OLED stats will be reset

/*
    BEGIN USER SETTINGS
*/
// These are the default settings that can only be changed by reflashing the Pico
const float VREF_VOLTAGE = 3.0;        // ADC reference voltage, defaults 3.3, with reference 3.0
const uint8_t ADC_RES = 12;            // Use 12-bit ADC resolution
const uint32_t EVENT_BUFFER = 100000;  // Buffer this many events for Serial.print
const uint8_t SCREEN_WIDTH = 128;      // OLED display width, in pixels
const uint8_t SCREEN_HEIGHT = 64;      // OLED display height, in pixels
const uint8_t SCREEN_ADDRESS = 0x3C;   // See datasheet for Address; 0x3D for 128x64, 0x3C for 128x32
const uint8_t TRNG_BITS = 8;           // Number of bits for each random number, max 8
//const uint8_t BASELINE_NUM = 100;      // Number of measurements taken to determine the DC baseline

struct Config {
  // These are the default settings that can also be changes via the serial commands
  bool ser_output = true;       // Wheter data should be Serial.println'ed
  bool geiger_mode = false;     // Measure only cps, not energy
  bool print_spectrum = false;  // Print the finishes spectrum, not just
  size_t meas_avg = 5;          // Number of meas. averaged each event, higher=longer dead time
  bool enable_display = false;  // Enable I2C Display, see settings above
  bool trng_enabled = false;    // Enable the True Random Number Generator

  // Do NOT modify this function:
  bool operator==(const Config &other) const {
    return (ser_output == other.ser_output && geiger_mode == other.geiger_mode && print_spectrum == other.print_spectrum && meas_avg == other.meas_avg && enable_display == other.enable_display && trng_enabled == other.trng_enabled);
  }
};
/*
   END USER SETTINGS
*/

volatile uint32_t spectrum[uint16_t(pow(2, ADC_RES))];  // Holds the spectrum histogram written to flash
volatile uint16_t events[EVENT_BUFFER];                 // Buffer array for single events
volatile uint16_t event_position = 0;                   // Target index in events array
uint32_t last_time = 0;                                 // Last timestamp for OLED refresh

volatile uint32_t trng_stamps[3];          // Timestamps for True Random Number Generator
volatile uint8_t trng_index = 0;           // Timestamp index for True Random Number Generator
volatile uint8_t random_num = 0b00000000;  // Generated random bits that form a byte together
volatile uint8_t bit_index = 0;            // Bit index for the generated number
volatile uint32_t trng_nums[1000];         // TRNG number output array
volatile uint16_t number_index = 0;        // Amount of saved numbers to the TRNG array

volatile float deadtime_avg = 0;   // Average detector dead time in µs
volatile uint32_t dt_avg_num = 0;  // Number of dead time measurements

//uint8_t baseline_index = 0;
//uint16_t baselines[BASELINE_NUM];
uint16_t current_baseline = 0;
bool adc_lock = false;

Config conf;  // Configuration object
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
//ADCInput sipm(AMP_PIN);


void serialInterruptMode(String *args) {
  String command = *args;
  command.replace("set int", "");
  command.trim();

  if (command == "spectrum") {
    conf.ser_output = true;
    conf.print_spectrum = true;
    println("Set serial interrupt mode to full spectrum.");
  } else if (command == "events") {
    conf.ser_output = true;
    conf.print_spectrum = false;
    println("Set serial interrupt mode to events.");
  } else if (command == "disable") {
    conf.ser_output = false;
    conf.print_spectrum = false;
    println("Disabled serial interrupts.");
  } else {
    println("Invalid input '" + command + "'.", true);
    println("Must be 'spectrum', 'events' or 'disable'.", true);
    return;
  }
  saveSettings();
}


void setDisplay(String *args) {
  String command = *args;
  command.replace("set display", "");
  command.trim();

  if (command == "enable") {
    conf.enable_display = true;
    println("Enabled display output. You might need to reset the device.");
  } else if (command == "disable") {
    conf.enable_display = false;
    println("Disabled display output. You might need to reset the device.");
  } else {
    println("Invalid input '" + command + "'.", true);
    println("Must be 'enable' or 'disable'.", true);
    return;
  }
  saveSettings();
}


void toggleGeigerMode(String *args) {
  String command = *args;
  command.replace("set mode", "");
  command.trim();

  if (command == "geiger") {
    conf.geiger_mode = true;
    event_position = 0;
    println("Enabled geiger mode.");
  } else if (command == "energy") {
    conf.geiger_mode = false;
    event_position = 0;
    println("Enabled energy measuring mode.");
  } else {
    println("Invalid input '" + command + "'.", true);
    println("Must be 'geiger' or 'energy'.", true);
    return;
  }
  saveSettings();
}


void toggleTRNG(String *args) {
  String command = *args;
  command.replace("set trng", "");
  command.trim();

  if (command == "enable") {
    conf.trng_enabled = true;
    println("Enabled True Random Number Generator output.");
  } else if (command == "disable") {
    conf.trng_enabled = false;
    println("Disabled True Random Number Generator output.");
  } else {
    println("Invalid input '" + command + "'.", true);
    println("Must be 'enable' or 'disable'.", true);
    return;
  }
  saveSettings();
}


void measAveraging(String *args) {
  String command = *args;
  command.replace("set averaging", "");
  command.trim();
  const long number = command.toInt();

  if (number > 0) {
    conf.meas_avg = number;
    println("Set measurement averaging to " + String(number) + ".");
  } else {
    println("Invalid input '" + command + "'.", true);
    println("Parameter must be a number >= 0.", true);
    return;
  }
  saveSettings();
}


void deviceInfo([[maybe_unused]] String *args) {
  println("=========================");
  println("-- Open Gamma Detector --");
  println("By NuclearPhoenix, Open Gamma Project");
  println("2023. https://github.com/OpenGammaProject");
  println("Firmware Version: " + FWVERS);
  println("=========================");
  println("Runtime: " + String(millis() / 1000.0) + " s");
  println("Avg. Dead Time: " + String(deadtime_avg) + " µs");

  const float deadtime_frac = float(deadtime_avg * dt_avg_num) / 1000.0 / float(millis()) * 100.0;

  println("Total Dead Time: " + String(deadtime_frac) + " %");
  println("CPU Frequency: " + String(rp2040.f_cpu() / 1e6) + " MHz");
  println("Used Heap Memory: " + String(rp2040.getUsedHeap() / 1000.0) + " kB");
  println("Free Heap Memory: " + String(rp2040.getFreeHeap() / 1000.0) + " kB");
  println("Total Heap Size: " + String(rp2040.getTotalHeap() / 1000.0) + " kB");
  println("Temperature: " + String(round(analogReadTemp(VREF_VOLTAGE) * 10.0) / 10.0, 1) + " °C");
  println("USB Connection: " + String(digitalRead(VBUS_MEAS)));

  const float v = 3.0 * analogRead(VSYS_MEAS) * VREF_VOLTAGE / (pow(2, ADC_RES) - 1);

  println("Supply Voltage: " + String(round(v * 10.0) / 10.0, 1) + " V");
}


void fsInfo([[maybe_unused]] String *args) {
  FSInfo fsinfo;
  LittleFS.info(fsinfo);
  println("Total Size: " + String(fsinfo.totalBytes / 1000.0) + " kB");
  print("Used Size: " + String(fsinfo.usedBytes / 1000.0) + " kB");
  cleanPrintln(" / " + String(float(fsinfo.usedBytes) / fsinfo.totalBytes * 100) + " %");
  println("Block Size: " + String(fsinfo.blockSize / 1000.0) + " kB");
  println("Page Size: " + String(fsinfo.pageSize) + " B");
  println("Max Open Files: " + String(fsinfo.maxOpenFiles));
  println("Max Path Length: " + String(fsinfo.maxPathLength));
}


void getSpectrumData([[maybe_unused]] String *args) {
  for (size_t i = 0; i < pow(2, ADC_RES); i++) {
    cleanPrint(String(spectrum[i]) + ";");
  }
  cleanPrintln();
}


void clearSpectrumData([[maybe_unused]] String *args) {
  println("Clearing spectrum...");
  clearSpectrum();
  last_time = millis();
  println("Cleared!");
}


void readSettings([[maybe_unused]] String *args) {
  File saveFile = LittleFS.open("/config.json", "r");

  if (!saveFile) {
    println("Could not open save file!", true);
    return;
  }

  String file = "";

  while (saveFile.available()) {
    file += saveFile.readString();
  }

  saveFile.close();

  DynamicJsonDocument doc(1024);
  DeserializationError error = deserializeJson(doc, file);

  if (error) {
    print("Could not load config from json file: ", true);
    cleanPrintln(error.f_str());
    return;
  }

  serializeJsonPretty(doc, Serial);

  cleanPrintln();
  println("Read settings file successfully.");
}


void resetSettings([[maybe_unused]] String *args) {
  Config defaultConf;
  conf = defaultConf;
  println("Applied default settings.");
  println("You might need to reboot for all changes to take effect.");

  saveSettings();
}


void rebootNow([[maybe_unused]] String *args) {
  println("You might need to reconnect after reboot.");
  println("Rebooting now...");
  delay(1000);
  rp2040.reboot();
}


void clearSpectrum() {
  for (size_t i = 0; i < pow(2, ADC_RES); i++) {
    spectrum[i] = 0;
  }
}


void serialEvent() {
  Shell.handleEvent();  // Handle the serial input for the USB Serial
}


void serialEvent2() {
  Shell.handleEvent();  // Handle the serial input for the Hardware Serial
}


Config loadSettings(bool msg = true) {
  Config new_conf;
  File saveFile = LittleFS.open("/config.json", "r");

  if (!saveFile) {
    println("Could not open save file!", true);
    return new_conf;
  }

  String json = "";

  while (saveFile.available()) {
    json += saveFile.readString();
  }

  saveFile.close();

  DynamicJsonDocument doc(1024);
  DeserializationError error = deserializeJson(doc, json);

  if (error) {
    print("Could not load config from json file: ", true);
    cleanPrintln(error.f_str());
    return new_conf;
  }

  new_conf.ser_output = doc["ser_output"];
  new_conf.geiger_mode = doc["geiger_mode"];
  new_conf.print_spectrum = doc["print_spectrum"];
  new_conf.meas_avg = doc["meas_avg"];
  new_conf.enable_display = doc["enable_display"];
  new_conf.trng_enabled = doc["trng_enabled"];

  if (msg) {
    println("Successfuly loaded settings from flash.");
  }
  return new_conf;
}


bool saveSettings() {
  Config read_conf = loadSettings(false);

  if (read_conf == conf) {
    //println("Settings did not change... not writing to flash.");
    return false;
  }

  File saveFile = LittleFS.open("/config.json", "w");

  if (!saveFile) {
    println("Could not open save file!", true);
    return false;
  }

  DynamicJsonDocument doc(1024);

  doc["ser_output"] = conf.ser_output;
  doc["geiger_mode"] = conf.geiger_mode;
  doc["print_spectrum"] = conf.print_spectrum;
  doc["meas_avg"] = conf.meas_avg;
  doc["enable_display"] = conf.enable_display;
  doc["trng_enabled"] = conf.trng_enabled;

  serializeJson(doc, saveFile);

  saveFile.close();
  //println("Successfuly written config to flash.");
  return true;
}


float readTemp() {
  adc_lock = true;                         // Flag this, so that nothing else uses the ADC in the mean time
  delayMicroseconds(conf.meas_avg * 100);  // Wait for an already-executing interrupt
  const float temp = analogReadTemp(VREF_VOLTAGE);
  adc_lock = false;
  return temp;
}


void resetSampleHold() {  // Reset sample and hold circuit
  digitalWrite(RST_PIN, HIGH);
  delayMicroseconds(1);  // Discharge for 1 µs, actually takes 2 µs - enough for a discharge
  digitalWrite(RST_PIN, LOW);
}


void drawSpectrum() {
  const uint16_t BINSIZE = floor(pow(2, ADC_RES) / SCREEN_WIDTH);
  uint32_t eventBins[SCREEN_WIDTH];
  uint16_t offset = 0;
  uint32_t max_num = 0;
  uint32_t total = 0;

  for (uint16_t i = 0; i < SCREEN_WIDTH; i++) {
    uint32_t totalValue = 0;

    for (uint16_t j = offset; j < offset + BINSIZE; j++) {
      totalValue += spectrum[j];
    }

    offset += BINSIZE;
    eventBins[i] = totalValue;

    if (totalValue > max_num) {
      max_num = totalValue;
    }

    total += totalValue;
  }

  float scale_factor = 0.0;

  if (max_num > 0) {  // No events accumulated, catch divide by zero
    scale_factor = float(SCREEN_HEIGHT - 11) / float(max_num);
  }

  if (millis() < last_time) {  // Catch Millis() Rollover
    last_time = millis();
    return;
  }

  uint32_t time_delta = millis() - last_time;

  if (time_delta == 0) {  // Catch divide by zero
    time_delta = 1000;
  }

  display.clearDisplay();
  display.setCursor(0, 0);

  display.print(total * 1000.0 / time_delta);
  display.print(" cps");

  const int16_t temp = round(readTemp());

  if (temp < 0) {
    display.setCursor(SCREEN_WIDTH - 30, 0);
  } else {
    display.setCursor(SCREEN_WIDTH - 24, 0);
  }
  display.print(temp);
  display.println(" C");

  const uint32_t seconds_running = round(time_delta / 1000.0);
  const uint8_t char_offset = floor(log10(seconds_running));

  display.setCursor(SCREEN_WIDTH - 18 - char_offset * 6, 8);
  display.print(seconds_running);
  display.println(" s");

  for (uint16_t i = 0; i < SCREEN_WIDTH; i++) {
    uint32_t val = round(eventBins[i] * scale_factor);
    display.drawFastVLine(i, SCREEN_HEIGHT - val - 1, val, SSD1306_WHITE);
  }
  display.drawFastHLine(0, SCREEN_HEIGHT - 1, SCREEN_WIDTH, SSD1306_WHITE);

  display.display();

  if (total > EVT_RESET_C) {
    last_time = millis();
    clearSpectrum();
  }
}


void eventInt() {
  const uint32_t start = micros();

  digitalWrite(LED, HIGH);  // Activity LED

  uint16_t mean = 0;

  if (!conf.geiger_mode && !adc_lock) {
    uint16_t meas[conf.meas_avg];

    for (size_t i = 0; i < conf.meas_avg; i++) {
      meas[i] = analogRead(AIN_PIN);
    }

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
      avg = 0.0;
    } else {
      avg /= conf.meas_avg - invalid;
    }

    // Subtract DC bias from pulse avg and then convert float --> uint16_t ADC channel
    mean = round(avg - current_baseline);
    // Use median instead of average?

    /*
      float var = 0.0;
      for (size_t i = 0; i < conf.meas_avg; i++) {
      var += sq(meas[i] - mean);
      }
      var /= conf.meas_avg;
    */
  }

  if (conf.ser_output || conf.enable_display) {
    events[event_position] = mean;
    spectrum[mean] += 1;
    //cleanPrint(' ' + String(sqrt(var)) + ';');
    //cleanPrintln(' ' + String(sqrt(var)/mean) + ';');
    if (event_position >= EVENT_BUFFER - 1) {  // Increment if memory available, else overwrite array
      event_position = 0;
    } else {
      event_position++;
    }
  }

  resetSampleHold();

  if (conf.trng_enabled) {
    // Calculations for the TRNG
    trng_stamps[trng_index] = micros();

    if (trng_index < 2) {
      trng_index++;
    } else {
      // Catch micros() overflow
      if (trng_stamps[1] > trng_stamps[0] && trng_stamps[2] > trng_stamps[1]) {
        uint32_t delta0 = trng_stamps[1] - trng_stamps[0];
        uint32_t delta1 = trng_stamps[2] - trng_stamps[1];

        if (delta0 < delta1) {
          bitWrite(random_num, bit_index, 0);
        } else {
          bitWrite(random_num, bit_index, 1);
        }

        if (bit_index < TRNG_BITS - 1) {
          bit_index++;
        } else {
          trng_nums[number_index] = random_num;

          if (number_index < 999) {
            number_index++;
          } else {
            number_index = 0;  // Catch overflow
          }

          random_num = 0b00000000;  // Clear number
          bit_index = 0;
        }
      }

      trng_index = 0;
    }
  }

  digitalWrite(LED, LOW);

  // Compute Detector Dead Time
  const uint32_t dt = micros() - start;
  dt_avg_num++;
  deadtime_avg += float(dt - deadtime_avg) / float(dt_avg_num);

  if (dt_avg_num >= pow(2, 32) - 2) {  // Catch dead time number overflow
    dt_avg_num = 0;
    deadtime_avg = 0;
  }
}

/*
    SETUP FUNCTIONS
*/
void setup() {
  rp2040.wdt_begin(2000);  // Enable hardware watchdog to check every 2s

  pinMode(AMP_PIN, INPUT);
  pinMode(INT_PIN, INPUT);
  pinMode(RST_PIN, OUTPUT_12MA);
  pinMode(AIN_PIN, INPUT);
  pinMode(LED, OUTPUT);

  analogReadResolution(ADC_RES);

  resetSampleHold();  // Reset before enabling the interrupts to avoid jamming

  //sipm.setBuffers(4, 64);
  //sipm.begin(1000000);

  attachInterrupt(digitalPinToInterrupt(INT_PIN), eventInt, FALLING);
}


void setup1() {
  pinMode(PS_PIN, OUTPUT_4MA);

  // Disable "Power-Saving" power supply option.
  // -> does not actually significantly save power, but output is much less noise this way!
  digitalWrite(PS_PIN, HIGH);

  pinMode(GND_PIN, INPUT);
  pinMode(VSYS_MEAS, INPUT);
  pinMode(VBUS_MEAS, INPUT);

  Shell.registerCommand(new ShellCommand(getSpectrumData, "read spectrum", "Read the spectrum histogram collected since the last reset."));
  Shell.registerCommand(new ShellCommand(readSettings, "read settings", "Read the current settings (file)."));
  Shell.registerCommand(new ShellCommand(deviceInfo, "read info", "Read misc info about the firmware and state of the device."));
  Shell.registerCommand(new ShellCommand(fsInfo, "read fs", "Read misc info about the used filesystem."));

  Shell.registerCommand(new ShellCommand(toggleTRNG, "set trng", "<toggle> Either 'enable' or 'disable' to enable/disable the true random number generator output."));
  Shell.registerCommand(new ShellCommand(setDisplay, "set display", "<toggle> Either 'enable' or 'disable' to enable or force disable OLED support."));
  Shell.registerCommand(new ShellCommand(toggleGeigerMode, "set mode", "<mode> Either 'geiger' or 'energy' to disable or enable energy measurements. Geiger mode only counts cps, but has ~3x higher saturation."));
  Shell.registerCommand(new ShellCommand(serialInterruptMode, "set int", "<mode> Either 'events', 'spectrum' or 'disable'. 'events' prints events as they arrive, 'spectrum' prints the accumulated histogram."));
  Shell.registerCommand(new ShellCommand(measAveraging, "set averaging", "<number> Number of ADC averages for each energy measurement. Takes ints, minimum is 1."));

  Shell.registerCommand(new ShellCommand(clearSpectrumData, "clear spectrum", "Clear the on-board spectrum hist."));
  Shell.registerCommand(new ShellCommand(resetSettings, "clear settings", "Clear all the settings and revert them back to default values."));
  Shell.registerCommand(new ShellCommand(rebootNow, "reboot", "Reboot the device."));

  // Starts FileSystem, autoformats if no FS is detected
  LittleFS.begin();
  conf = loadSettings();  // Read all the detector settings from flash

  // Set the correct SPI pins
  SPI.setRX(4);
  SPI.setTX(3);
  SPI.setSCK(2);
  SPI.setCS(5);
  // Set the correct I2C pins
  Wire.setSDA(0);
  Wire.setSCL(1);
  // Set the correct UART pins
  Serial2.setRX(9);
  Serial2.setTX(8);

  Shell.begin(2000000);
  Serial2.begin(2000000);

  println("Welcome to the Open Gamma Detector!");
  println("Firmware Version " + FWVERS);

  if (conf.enable_display) {
    if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
      println("Failed communication with the display. Maybe the I2C address is incorrect?", true);
      conf.enable_display = false;
    } else {
      display.setRotation(2);
      display.setTextSize(2);  // Draw 2X-scale text
      display.setTextColor(SSD1306_WHITE);

      display.clearDisplay();
      display.println("Open Gamma Detector");
      display.println();
      display.setTextSize(1);
      display.print("FW ");
      display.println(FWVERS);
      display.display();
      delay(2000);
    }
  }

  /*
  analogWriteFreq(1000);
  analogWriteRange(300);
  analogWrite(5, 150);
  */
}

/*
   LOOP FUNCTIONS
*/
void loop() {
  static uint32_t last_exec = 0;

  if (micros() - last_exec >= 500) {
    resetSampleHold();
    last_exec = micros();
  }

  // Compute the median DC baseline to subtract from each pulse
  /*
  baselines[baseline_index] = analogRead(AIN_PIN);
  baseline_index++;
  if (baseline_index >= BASELINE_NUM) {
    Array_Stats<uint16_t> Data_Array(baselines, sizeof(baselines) / sizeof(baselines[0]));
    current_baseline = round(Data_Array.Quartile(2));
    //current_baseline = round(Data_Array.Average(Data_Array.Arithmetic_Avg));
    baseline_index = 0;
  }
  */

  /*
  if (sipm.available() > 0) {
    const uint16_t data = sipm.read();
    if (data > 150) {
      println(data);
    }
  }
  */

  rp2040.wdt_reset();  // Reset watchdog, everything is fine

  delayMicroseconds(500);
}


void loop1() {
  if (conf.ser_output) {
    if (Serial || Serial2) {
      if (conf.print_spectrum) {
        for (uint16_t index = 0; index < uint16_t(pow(2, ADC_RES)); index++) {
          cleanPrint(String(spectrum[index]) + ";");
          //spectrum[index] = 0; // Uncomment for differential histogram
        }
      } else if (event_position > 0 && event_position <= EVENT_BUFFER) {
        for (uint16_t index = 0; index < event_position; index++) {
          cleanPrint(String(events[index]) + ";");
        }
      }
      cleanPrintln();
    }

    event_position = 0;
  }

  if (conf.trng_enabled) {
    if (Serial || Serial2) {
      for (size_t i = 0; i < number_index; i++) {
        cleanPrint(trng_nums[i], DEC);
        cleanPrintln(";");
      }
      number_index = 0;
    }
  }

  if (conf.enable_display) {
    drawSpectrum();
  }

  /*
  if (BOOTSEL) {
    // Do something when BOOTSEL button is pressed
    while (BOOTSEL) { // Wait for BOOTSEL to be released
      delay(1);
    }
  }
  */

  delay(1000);  // Wait for 1 sec, better: sleep for power saving?!
}
