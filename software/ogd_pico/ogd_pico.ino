/*

  Open Gamma Detector Sketch
  Only works on the Raspberry Pi Pico 2 (!) with arduino-pico!

  Triggers on newly detected pulses and measures their energy.

  2024, NuclearPhoenix. Open Gamma Project.
  https://github.com/OpenGammaProject/Open-Gamma-Detector

  ## NOTE:
  ## Only change the highlighted USER SETTINGS below
  ## except you know exactly what you are doing!
  ## Flash with default settings and
  ##   Flash Size: "4MB (Sketch: 1MB, FS: 3MB)"
  

  TODO: Add cps line trend to geiger mode
  TODO: Add custom display font

  TODO: Restructure files to make ino better readable (also Baseline, Recorder and TRNG classes?)

*/

#define _TASK_SCHEDULING_OPTIONS
#define _TASK_TIMECRITICAL       // Enable monitoring scheduling overruns
#define _TASK_SLEEP_ON_IDLE_RUN  // Enable 1 ms SLEEP_IDLE powerdowns between tasks if no callback methods were invoked during the pass
#include <TaskScheduler.h>       // Periodically executes tasks

#include "hardware/vreg.h"  // Used for vreg_set_voltage
#include "Helper.h"         // Misc helper functions
#include "FS.h"             // Functions for the LittleFS filesystem

#include <SimpleShell_Enhanced.h>  // Serial Commands/Console
#include <ArduinoJson.h>           // Load and save the settings file
#include <LittleFS.h>              // Used for FS, stores the settings and debug files
#include <RunningMedian.h>         // Used to get running median and average with circular buffers

/*
    ===================
    BEGIN USER SETTINGS
    ===================
*/
// These are the default settings that can only be changed by reflashing the Pico
#define SCREEN_TYPE SCREEN_SSD1306  // Display type: Either SCREEN_SSD1306 or SCREEN_SH1106
#define SCREEN_WIDTH 128            // OLED display width, in pixels
#define SCREEN_HEIGHT 64            // OLED display height, in pixels
#define SCREEN_ADDRESS 0x3C         // See datasheet for Address; 0x3D for 128x64, 0x3C for 128x32
#define EVENT_BUFFER 50000          // Buffer this many events for Serial.print
#define TRNG_BITS 8                 // Number of bits for each random number, max 8
#define BASELINE_NUM 101            // Number of measurements taken to determine the DC baseline
#define CONFIG_FILE "/config.json"  // File to store the settings
#define DEBUG_FILE "/debug.json"    // File to store some misc debug info
#define DISPLAY_REFRESH 1000        // Milliseconds between display refreshs
#define BUZZER_FREQ 2700            // Resonance frequency of the buzzer
#define AUTOSAVE_TIME 900000        // 15 Minutes in ms, time between recording autosaves
// Display Types: Select only one! One of them must be true and the other one false
#define SCREEN_SH1106 false
#define SCREEN_SSD1306 true

struct Config {
  // These are the default settings that can also be changes via the serial commands
  // Do not touch the struct itself, but only the values of the variables!
  bool ser_output = true;         // Wheter data should be Serial.println'ed
  bool geiger_mode = false;       // Measure only cps, not energy
  bool print_spectrum = false;    // Print the finishes spectrum, not just chronological events
  size_t meas_avg = 2;            // Number of meas. averaged each event, higher=longer dead time
  bool enable_display = false;    // Enable I2C Display, see settings above
  bool enable_trng = false;       // Enable the True Random Number Generator
  bool subtract_baseline = true;  // Subtract the DC bias from each pulse
  bool enable_ticker = false;     // Enable the buzzer to be used as a ticker for pulses
  size_t tick_rate = 20;          // Buzzer ticks once every tick_rate pulses

  // Do NOT modify the following operator function
  bool operator==(const Config &other) const {
    return (tick_rate == other.tick_rate && enable_ticker == other.enable_ticker && ser_output == other.ser_output && geiger_mode == other.geiger_mode && print_spectrum == other.print_spectrum && meas_avg == other.meas_avg && enable_display == other.enable_display && enable_trng == other.enable_trng && subtract_baseline == other.subtract_baseline);
  }
};
/*
    =================
    END USER SETTINGS
    =================
*/

const String FW_VERSION = "4.3.1";  // Firmware Version Code

const uint8_t GND_PIN = A2;    // GND meas pin
const uint8_t VSYS_MEAS = A3;  // VSYS/3
const uint8_t VBUS_MEAS = 24;  // VBUS Sense Pin
const uint8_t PS_PIN = 23;     // SMPS power save pin

const uint8_t AIN_PIN = A1;     // Analog input pin
const uint8_t AMP_PIN = A0;     // Preamp (baseline) meas pin
const uint8_t INT_PIN = 18;     // Signal interrupt pin
const uint8_t RST_PIN = 22;     // Peak detector MOSFET reset pin
const uint8_t LED = 25;         // Built-in LED on GP25
const uint8_t BUZZER_PIN = 7;   // Buzzer PWM pin for the ticker
const uint8_t BUTTON_PIN = 14;  // Misc button pin

const uint8_t LONG_PRESS = 10;  // Time until considered long button press

const uint8_t BUZZER_TICK = 10;     // On-time of the buzzer for a single pulse in ms
const uint16_t EVT_RESET_C = 3000;  // Number of counts after which the OLED stats will be reset
const uint16_t OUT_REFRESH = 1000;  // Milliseconds between serial data outputs

const float VREF_VOLTAGE = 3.0;             // ADC reference voltage, default is 3.0 with reference
const uint8_t ADC_RES = 12;                 // Use 12-bit ADC resolution
const uint16_t ADC_BINS = 0x01 << ADC_RES;  // Number of ADC bins

volatile uint32_t spectrum[ADC_BINS];          // Holds the output histogram (spectrum)
volatile uint32_t display_spectrum[ADC_BINS];  // Holds the display histogram (spectrum)

volatile uint16_t events[EVENT_BUFFER];  // Buffer array for single events
volatile uint32_t event_position = 0;    // Target index in events array
volatile unsigned long start_time = 0;   // Time in ms when the spectrum collection has started
volatile unsigned long last_time = 0;    // Last time the display has been refreshed
volatile uint32_t last_total = 0;        // Last total pulse count for display

volatile unsigned long trng_stamps[3];     // Timestamps for True Random Number Generator
volatile uint8_t random_num = 0b00000000;  // Generated random bits that form a byte together
volatile uint8_t bit_index = 0;            // Bit index for the generated number
volatile uint32_t trng_nums[1000];         // TRNG number output array
volatile uint16_t number_index = 0;        // Amount of saved numbers to the TRNG array
volatile uint32_t total_events = 0;        // Total number of all registered pulses

RunningMedian baseline(BASELINE_NUM);  // Array of a number of baseline (DC bias) measurements at the SiPM input
uint16_t current_baseline = 0;         // Median value of the input baseline voltage

uint32_t recordingDuration = 0;        // Duration of the spectrum recording in seconds
String recordingFile = "";             // Filename for the spectrum recording file
volatile bool isRecording = false;     // Currently running a recording
unsigned long recordingStartTime = 0;  // Start timestamp of the recording in ms

volatile bool adc_lock = false;  // Locks the ADC if it's currently in use

// Stores 5 * DISPLAY_REFRESH worth of "current" cps to calculate an average cps value in a ring buffer config
RunningMedian counts(5);

// Stores the last
RunningMedian dead_time(100);

// Configuration struct with all user settings
Config conf;

// Set up serial shells
CShell shell(Serial);
CShell shell1(Serial2);

// Check for the right display type
#if (SCREEN_TYPE == SCREEN_SH1106)
#include <Adafruit_SH110X.h>
#define DISPLAY_WHITE SH110X_WHITE
Adafruit_SH1106G display = Adafruit_SH1106G(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
#elif (SCREEN_TYPE == SCREEN_SSD1306)
#include <Adafruit_SSD1306.h>
#define DISPLAY_WHITE SSD1306_WHITE
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
#endif

// Forward declaration of callbacks
void writeDebugFileTime();
void queryButton();
void updateDisplay();
void dataOutput();
void updateBaseline();
void resetPHCircuit();
void recordCycle();

// Tasks
Task writeDebugFileTimeTask(60 * 60 * 1000, TASK_FOREVER, &writeDebugFileTime);
Task queryButtonTask(100, TASK_FOREVER, &queryButton);
Task updateDisplayTask(DISPLAY_REFRESH, TASK_FOREVER, &updateDisplay);
Task dataOutputTask(OUT_REFRESH, TASK_FOREVER, &dataOutput);
Task updateBaselineTask(1, TASK_FOREVER, &updateBaseline);
Task resetPHCircuitTask(1, TASK_FOREVER, &resetPHCircuit);
Task recordCycleTask(60000, 0, &recordCycle);

// Scheduler
Scheduler schedule;

// Declare CommandFunction type
using CommandFunction = void (*)(String *);

// Define a struct for command information
struct CommandInfo {
  CommandFunction function;
  const char *name;
  const char *description;
};


void clearSpectrum() {
  for (size_t i = 0; i < ADC_BINS; i++) {
    spectrum[i] = 0;
  }
}


void clearSpectrumDisplay() {
  for (size_t i = 0; i < ADC_BINS; i++) {
    display_spectrum[i] = 0;
  }
  start_time = millis();  // Spectrum pulse collection has started
  last_time = millis();
  last_total = 0;  // Remove old values
}


void resetSampleHold(uint8_t time = 2) {  // Reset sample and hold circuit
  digitalWriteFast(RST_PIN, HIGH);
  delayMicroseconds(time);  // Discharge for (default) 2 µs -> ~99% discharge time for 1 kOhm and 470 pF
  digitalWriteFast(RST_PIN, LOW);
}


void resetPHCircuit() {
  if (!adc_lock) {
    adc_lock = true;    // Disable interrupt ADC measurements while resetting
    resetSampleHold();  // Periodically reset the S&H/P&H circuit
    adc_lock = false;
  }
  // TODO: Check if adc was locked and decrease interval to compensate
}


void queryButton() {
  static uint16_t pressed = 0;
  static bool press_lock = false;

  if (digitalRead(BUTTON_PIN) == LOW) {
    pressed++;

    if (pressed >= LONG_PRESS && !press_lock) {
      /*
        Long Press: Toggle Buzzer
      */
      conf.enable_ticker = !conf.enable_ticker;

      println(conf.enable_ticker ? "Enabled ticker output." : "Disabled ticker output.");

      updateDisplayTask.forceNextIteration();  // Update the display immediately

      saveSettings();  // Saved updated settings
      // Introduce some dead time after a button press
      // Fixes a bug that would make the buzzer go crazy
      // Idk what's going on, but it's related to the saveSettings() function
      // Maybe the flash is too slow or something
      delay(100);

      press_lock = true;
    }
  } else {
    if (pressed > 0) {
      if (pressed < LONG_PRESS) {
        /*
          Short Press: Switch Modes
        */
        conf.geiger_mode = !conf.geiger_mode;
        event_position = 0;
        clearSpectrum();
        clearSpectrumDisplay();
        resetSampleHold();

        println(conf.geiger_mode ? "Switched to geiger mode." : "Switched to energy measuring mode.");

        updateDisplayTask.forceNextIteration();  // Update the display immediately

        saveSettings();  // Saved updated settings
        // Introduce some dead time after a button press
        // Fixes a bug that would make the buzzer go crazy
        // Idk what's going on, but it's related to the saveSettings() function
        // Maybe the flash is too slow or something
        delay(100);
      }
    }

    pressed = 0;
    press_lock = false;
  }

  if (BOOTSEL) {
    rp2040.rebootToBootloader();
  }

  rp2040.wdt_reset();  // Reset watchdog, everything is fine
}


void dataOutput() {
  // MAYBE ONLY START TASK IF OUTPUT IS ENABLED?
  if (conf.ser_output) {
    if (Serial || Serial2) {
      if (conf.print_spectrum) {
        for (uint16_t index = 0; index < ADC_BINS; index++) {
          cleanPrint(String(spectrum[index]) + ";");
          //spectrum[index] = 0; // Uncomment for differential histogram
        }
        cleanPrintln();
      } else if (event_position > 0 && event_position <= EVENT_BUFFER) {
        for (uint16_t index = 0; index < event_position; index++) {
          cleanPrint(String(events[index]) + ";");
        }
        cleanPrintln();
      }
    }

    event_position = 0;
  }

  // MAYBE SEPARATE TRNG AND SERIAL OUTPUT TASKS?
  if (conf.enable_trng) {
    if (Serial || Serial2) {
      for (size_t i = 0; i < number_index; i++) {
        cleanPrint(trng_nums[i], DEC);
        cleanPrintln(";");
      }
      number_index = 0;
    }
  }
}


void updateBaseline() {
  static uint8_t baseline_done = 0;

  // Compute the median DC baseline to subtract from each pulse
  if (conf.subtract_baseline) {
    if (!adc_lock) {
      adc_lock = true;  // Disable interrupt ADC measurements while resetting
      baseline.add(analogRead(AIN_PIN));
      adc_lock = false;

      baseline_done++;

      if (baseline_done >= BASELINE_NUM) {
        current_baseline = round(baseline.getMedian());  // Take the median value

        baseline_done = 0;
      }
    }
  }
}


float readTemp() {
  adc_lock = true;                        // Flag this, so that nothing else uses the ADC in the mean time
  delayMicroseconds(conf.meas_avg * 20);  // Wait for an already-executing interrupt
  const float temp = analogReadTemp(VREF_VOLTAGE);
  adc_lock = false;
  return temp;
}


void recordCycle() {
  static unsigned long saveTime = 0;
  static uint32_t recordingSpectrum[ADC_BINS];

  const unsigned long nowTime = millis();

  if (!isRecording) {
    isRecording = true;
    recordingStartTime = nowTime;
    saveTime = nowTime;

    // Copy original spectrum data into recordingSpectrum
    for (uint16_t i = 0; i < ADC_BINS; i++) {
      recordingSpectrum[i] = spectrum[i];
    }
  }

  isRecording = !recordCycleTask.isLastIteration();

  // Last iteration or autosave interval -> save to file
  if (!isRecording || (nowTime - saveTime >= AUTOSAVE_TIME)) {
    // Generate current recording spectrum in NPESv2 format
    JsonDocument doc;

    doc["schemaVersion"] = "NPESv2";

    JsonObject data_0 = doc["data"].add<JsonObject>();

    JsonObject data_0_deviceData = data_0["deviceData"].to<JsonObject>();
    data_0_deviceData["softwareName"] = "OGD FW " + FW_VERSION;
    data_0_deviceData["deviceName"] = "Open Gamma Detector Rev. 4";

    JsonObject data_0_resultData_energySpectrum = data_0["resultData"]["energySpectrum"].to<JsonObject>();
    data_0_resultData_energySpectrum["numberOfChannels"] = ADC_BINS;
    data_0_resultData_energySpectrum["measurementTime"] = round((nowTime - recordingStartTime) / 1000.0);

    JsonArray data_0_resultData_energySpectrum_spectrum = data_0_resultData_energySpectrum["spectrum"].to<JsonArray>();

    uint32_t sum = 0;
    for (uint16_t i = 0; i < ADC_BINS; i++) {
      const uint32_t diff = spectrum[i] - recordingSpectrum[i];
      data_0_resultData_energySpectrum_spectrum.add(diff);
      sum += diff;
    }

    data_0_resultData_energySpectrum["validPulseCount"] = sum;

    doc.shrinkToFit();

    File saveFile = LittleFS.open(DATA_DIR_PATH + recordingFile, "w");  // Open read and write
    if (!saveFile) {
      println("Could not create file to save recording data!", true);
    }
    serializeJson(doc, saveFile);
    saveFile.close();

    saveTime = nowTime;
  }
}

/*
  BEGIN SERIAL COMMANDS
*/
void recordStart(String *args) {
  String command = *args;
  command.replace("record start", "");
  command.trim();

  if (isRecording) {
    println("Device is already recording! You must stop the current recording to start a new one.", true);
    return;
  }

  // Check and warn if the filesystem is almost full
  if (getUsedPercentageFS() > 90) {
    println("WARNING: Filesystem is almost full. Check if you have enough space for an additional recording.", true);
    println("The device might not be able to save the data. Delete old files if you need more free space.", true);
  }

  // Find the position of the first space
  const int spaceChar = command.indexOf(' ');

  // Extract the command
  String timeStr = command.substring(0, spaceChar);
  timeStr.trim();
  const long time = timeStr.toInt();

  if (time <= 0) {
    println("Invalid time input '" + command + "'.", true);
    println("Time parameter must be a number > 0.", true);
    return;
  }

  String filename = command.substring(spaceChar);
  filename.trim();

  if (filename == "") {
    println("Invalid file input '" + command + "'.", true);
    println("Filename must be a string with a non-zero length.", true);
    return;
  }

  recordingFile = filename + ".json";  // Force JSON file extension because of NPESv2
  recordingDuration = time;

  println("Starting recording to file '" + recordingFile + "' for " + String(recordingDuration) + " minutes.");
  println("You can always check out the current status or stop the recording.");

  // Set task duration and start the task
  recordCycleTask.setIterations(time + 1);  // Time is in minutes, task executes every minute, so time == iterations
  recordCycleTask.enable();
}


void recordStop([[maybe_unused]] String *args) {
  if (!isRecording) {
    println("No recording is currently running.", true);
    return;
  }

  const unsigned long runTime = recordCycleTask.getRunCounter();

  recordCycleTask.setIterations(1);  // Run for one last time
  recordCycleTask.restart();         // Run immediately

  println("Stopped spectrum recording to file '" + recordingFile + "' after " + String(runTime) + " minutes.");
}


void recordStatus([[maybe_unused]] String *args) {
  if (!recordCycleTask.isEnabled()) {
    println("No recording is currently running.", true);
    print("Last recording: ", true);
    cleanPrintln(recordingFile);
    return;
  }

  const float runTime = (millis() - recordingStartTime) / 1000.0 / 60.0;

  println("Recording Status: \tRunning...");
  print("Recording File: \t");
  cleanPrintln(recordingFile);
  print("Recording Time: \t");
  cleanPrint(runTime);
  cleanPrint(" / ");
  cleanPrint(recordingDuration);
  cleanPrintln(" minutes");
  print("Progress: \t\t");
  cleanPrint(runTime / recordingDuration * 100.0);
  cleanPrintln(" %");
}


void setSerialOutMode(String *args) {
  String command = *args;
  command.replace("set out", "");
  command.trim();

  if (command == "spectrum") {
    conf.ser_output = true;
    conf.print_spectrum = true;
    println("Set serial output mode to spectrum histogram.");
  } else if (command == "events") {
    conf.ser_output = true;
    conf.print_spectrum = false;
    println("Set serial output mode to events.");
  } else if (command == "off") {
    conf.ser_output = false;
    conf.print_spectrum = false;
    println("Disabled serial outputs.");
  } else {
    println("Invalid input '" + command + "'.", true);
    println("Must be 'spectrum', 'events' or 'off'.", true);
    return;
  }
  saveSettings();
}


void toggleDisplay(String *args) {
  String command = *args;
  command.replace("set display", "");
  command.trim();

  if (command == "on") {
    conf.enable_display = true;
    println("Enabled display output. You might need to reboot the device.");
  } else if (command == "off") {
    conf.enable_display = false;
    println("Disabled display output. You might need to reboot the device.");
  } else {
    println("Invalid input '" + command + "'.", true);
    println("Must be 'on' or 'off'.", true);
    return;
  }
  saveSettings();
}


void toggleTicker(String *args) {
  String command = *args;
  command.replace("set ticker", "");
  command.trim();

  if (command == "on") {
    conf.enable_ticker = true;
    println("Enabled ticker output.");
  } else if (command == "off") {
    conf.enable_ticker = false;
    println("Disabled ticker output.");
  } else {
    println("Invalid input '" + command + "'.", true);
    println("Must be 'on' or 'off'.", true);
    return;
  }
  saveSettings();
}


void setTickerRate(String *args) {
  String command = *args;
  command.replace("set tickrate", "");
  command.trim();

  const long number = command.toInt();

  if (number > 0) {
    conf.tick_rate = number;
    println("Set ticker rate to " + String(number) + ".");
    saveSettings();
  } else {
    println("Invalid input '" + command + "'.", true);
    println("Parameter must be a number > 0.", true);
  }
}


void setMode(String *args) {
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
  resetSampleHold();
  saveSettings();
}


void toggleTRNG(String *args) {
  String command = *args;
  command.replace("set trng", "");
  command.trim();

  if (command == "on") {
    conf.enable_trng = true;
    println("Enabled True Random Number Generator output.");
  } else if (command == "off") {
    conf.enable_trng = false;
    println("Disabled True Random Number Generator output.");
  } else {
    println("Invalid input '" + command + "'.", true);
    println("Must be 'on' or 'off'.", true);
    return;
  }
  saveSettings();
}


void toggleBaseline(String *args) {
  String command = *args;
  command.replace("set baseline", "");
  command.trim();

  if (command == "on") {
    conf.subtract_baseline = true;
    println("Enabled automatic DC bias subtraction.");
  } else if (command == "off") {
    conf.subtract_baseline = false;
    current_baseline = 0;  // Reset baseline back to zero
    println("Disabled automatic DC bias subtraction.");
  } else {
    println("Invalid input '" + command + "'.", true);
    println("Must be 'on' or 'off'.", true);
    return;
  }
  saveSettings();
}


void setMeasAveraging(String *args) {
  String command = *args;
  command.replace("set averaging", "");
  command.trim();
  const long number = command.toInt();

  if (number > 0) {
    conf.meas_avg = number;
    println("Set measurement averaging to " + String(number) + ".");
    saveSettings();
  } else {
    println("Invalid input '" + command + "'.", true);
    println("Parameter must be a number > 0.", true);
  }
}


void deviceInfo([[maybe_unused]] String *args) {
  File debugFile = LittleFS.open(DEBUG_FILE, "r");

  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, debugFile);

  const uint32_t power_cycle = (!debugFile || error) ? 0 : doc["power_cycle_count"];
  const uint32_t power_on = (!debugFile || error) ? 0 : doc["power_on_hours"];

  const float avg_dt = dead_time.getMedianAverage(50);

  debugFile.close();

  println("=========================");
  println("-- Open Gamma Detector --");
  println("By NuclearPhoenix, Open Gamma Project");
  println("2024. https://github.com/OpenGammaProject");
  println("Firmware Version: " + FW_VERSION);
  println("=========================");
  println("Runtime: \t\t" + String(millis() / 1000.0) + " s");
  print("Last reset reason: \t");

  cleanPrintln(RESET_REASON_TEXT[rp2040.getResetReason()]);  // Get reset reason text

  print("Average Dead Time: \t");

  cleanPrintln((total_events == 0) ? "n/a" : String(round(avg_dt), 0) + " µs");

  const float deadtime_frac = avg_dt * total_events / 1000.0 / float(millis()) * 100.0;

  print("Total Dead Time: \t");
  cleanPrintln(isnan(deadtime_frac) ? "n/a" : String(deadtime_frac) + " %");

  println("Total Pulses: \t" + String(total_events));
  println("CPU Frequency: \t" + String(rp2040.f_cpu() / 1e6) + " MHz");
  println("Used Heap Memory: \t" + String(rp2040.getUsedHeap() / 1000.0) + " kB / " + String(rp2040.getUsedHeap() * 100.0 / rp2040.getTotalHeap(), 0) + "%");
  println("Free Heap Memory: \t" + String(rp2040.getFreeHeap() / 1000.0) + " kB / " + String(rp2040.getFreeHeap() * 100.0 / rp2040.getTotalHeap(), 0) + "%");
  println("Temperature: \t" + String(round(readTemp() * 10.0) / 10.0, 1) + " °C");
  println("USB Connection: \t" + String(digitalRead(VBUS_MEAS)));

  const float v = 3.0 * analogRead(VSYS_MEAS) * VREF_VOLTAGE / (ADC_BINS - 1);

  println("Supply Voltage: \t" + String(round(v * 10.0) / 10.0, 1) + " V");

  print("Power Cycle Count: \t");
  cleanPrintln((power_cycle == 0) ? "n/a" : String(power_cycle));

  print("Power-on hours: \t");
  cleanPrintln((power_on == 0) ? "n/a" : String(power_on));
}


void getSpectrumData([[maybe_unused]] String *args) {
  cleanPrintln();
  println("Pulse height histogram:");
  for (size_t i = 0; i < ADC_BINS; i++) {
    cleanPrintln(spectrum[i]);
  }
  cleanPrintln();
  println("Hint: To import this data into Gamma MCA, you have to replace all the ';' with a new line '\n'.");
}


void clearSpectrumData([[maybe_unused]] String *args) {
  println("Resetting spectrum...");
  clearSpectrum();
  //clearSpectrumDisplay();
  println("Successfully reset spectrum!");
}


void readSettings([[maybe_unused]] String *args) {
  File saveFile = LittleFS.open(CONFIG_FILE, "r");

  if (!saveFile) {
    println("Could not open save file!", true);
    return;
  }

  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, saveFile);

  saveFile.close();

  if (error) {
    print("Could not load config from json file: ", true);
    cleanPrintln(error.f_str());
    return;
  }

  doc.shrinkToFit();  // Optional
  serializeJsonPretty(doc, Serial);

  cleanPrintln();
  println("Read settings file successfully.");
}


void resetSettings([[maybe_unused]] String *args) {
  Config defaultConf;  // New Config object with all default parameters
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


void serialEvent() {
  shell.handleEvent();  // Handle the serial input for the USB Serial
  //shell1.handleEvent();  // Handle the serial input for the Hardware Serial
}


void serialEvent2() {
  //shell.handleEvent();  // Handle the serial input for the USB Serial
  shell1.handleEvent();  // Handle the serial input for the Hardware Serial
}
/*
  END SERIAL COMMANDS
*/

/*
  BEGIN DEBUG AND SETTINGS
*/
void writeDebugFileTime() {
  // ALMOST THE SAME AS THE BOOT DEBUG FILE WRITE!
  File debugFile = LittleFS.open(DEBUG_FILE, "r");  // Open read and write

  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, debugFile);

  if (!debugFile || error) {
    //println("Could not open debug file!", true);
    print("Could not load debug info from json file: ", true);
    cleanPrintln(error.f_str());

    doc["power_cycle_count"] = 0;
    doc["power_on_hours"] = 0;
  }

  debugFile.close();

  const uint32_t temp = doc.containsKey("power_on_hours") ? doc["power_on_hours"] : 0;
  doc["power_on_hours"] = temp + 1;

  debugFile = LittleFS.open(DEBUG_FILE, "w");  // Open read and write
  doc.shrinkToFit();                           // Optional
  serializeJson(doc, debugFile);
  debugFile.close();
}


void writeDebugFileBoot() {
  // ALMOST THE SAME AS THE TIME DEBUG FILE WRITE!
  File debugFile = LittleFS.open(DEBUG_FILE, "r");  // Open read and write

  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, debugFile);

  if (!debugFile || error) {
    //println("Could not open debug file!", true);
    print("Could not load debug info from json file: ", true);
    cleanPrintln(error.f_str());

    doc["power_cycle_count"] = 0;
    doc["power_on_hours"] = 0;
  }

  debugFile.close();

  const uint32_t temp = doc.containsKey("power_cycle_count") ? doc["power_cycle_count"] : 0;
  doc["power_cycle_count"] = temp + 1;

  debugFile = LittleFS.open(DEBUG_FILE, "w");  // Open read and write
  doc.shrinkToFit();                           // Optional
  serializeJson(doc, debugFile);
  debugFile.close();
}


Config loadSettings(bool msg = true) {
  Config new_conf;
  File saveFile = LittleFS.open(CONFIG_FILE, "r");

  if (!saveFile) {
    println("Could not open save file! Creating a fresh file...", true);

    writeSettingsFile();  // Force creation of a new file

    return new_conf;
  }

  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, saveFile);

  saveFile.close();

  if (error) {
    print("Could not load config from json file: ", true);
    cleanPrintln(error.f_str());
    return new_conf;
  }

  if (doc.containsKey("ser_output")) {
    new_conf.ser_output = doc["ser_output"];
  }
  if (doc.containsKey("geiger_mode")) {
    new_conf.geiger_mode = doc["geiger_mode"];
  }
  if (doc.containsKey("print_spectrum")) {
    new_conf.print_spectrum = doc["print_spectrum"];
  }
  if (doc.containsKey("meas_avg")) {
    new_conf.meas_avg = doc["meas_avg"];
  }
  if (doc.containsKey("enable_display")) {
    new_conf.enable_display = doc["enable_display"];
  }
  if (doc.containsKey("enable_trng")) {
    new_conf.enable_trng = doc["enable_trng"];
  }
  if (doc.containsKey("subtract_baseline")) {
    new_conf.subtract_baseline = doc["subtract_baseline"];
  }
  if (doc.containsKey("enable_ticker")) {
    new_conf.enable_ticker = doc["enable_ticker"];
  }
  if (doc.containsKey("tick_rate")) {
    new_conf.tick_rate = doc["tick_rate"];
  }

  if (msg) {
    println("Successfuly loaded settings from flash.");
  }
  return new_conf;
}


bool writeSettingsFile() {
  File saveFile = LittleFS.open(CONFIG_FILE, "w");

  if (!saveFile) {
    println("Could not open save file!", true);
    return false;
  }

  JsonDocument doc;

  doc["ser_output"] = conf.ser_output;
  doc["geiger_mode"] = conf.geiger_mode;
  doc["print_spectrum"] = conf.print_spectrum;
  doc["meas_avg"] = conf.meas_avg;
  doc["enable_display"] = conf.enable_display;
  doc["enable_trng"] = conf.enable_trng;
  doc["subtract_baseline"] = conf.subtract_baseline;
  doc["enable_ticker"] = conf.enable_ticker;
  doc["tick_rate"] = conf.tick_rate;

  doc.shrinkToFit();  // Optional
  serializeJson(doc, saveFile);

  saveFile.close();

  return true;
}


bool saveSettings() {
  const Config read_conf = loadSettings(false);

  if (read_conf == conf) {
    //println("Settings did not change... not writing to flash.");
    return false;
  }

  //println("Successfuly written config to flash.");
  return writeSettingsFile();
}
/*
  END DEBUG AND SETTINGS
*/

/*
  BEGIN DISPLAY FUNCTIONS
*/
void updateDisplay() {
  // Update display every DISPLAY_REFRESH ms
  if (conf.enable_display) {
    conf.geiger_mode ? drawGeigerCounts() : drawSpectrum();
  }
}


void drawSpectrum() {
  const uint16_t BINSIZE = floor(ADC_BINS / SCREEN_WIDTH);
  uint32_t eventBins[SCREEN_WIDTH];
  uint16_t offset = 0;
  uint32_t max_num = 0;
  uint32_t total = 0;

  for (uint16_t i = 0; i < SCREEN_WIDTH; i++) {
    uint32_t totalValue = 0;

    for (uint16_t j = offset; j < offset + BINSIZE; j++) {
      totalValue += display_spectrum[j];
    }

    offset += BINSIZE;
    eventBins[i] = totalValue;

    if (totalValue > max_num) {
      max_num = totalValue;
    }

    total += totalValue;
  }

  const unsigned long now_time = millis();

  // No events accumulated, catch divide by zero
  const float scale_factor = (max_num > 0) ? float(SCREEN_HEIGHT - 11) / float(max_num) : 0.;

  const uint32_t new_total = total - last_total;
  last_total = total;

  if (now_time < last_time) {  // Catch Millis() Rollover
    last_time = now_time;
    return;
  }

  unsigned long time_delta = now_time - last_time;
  last_time = now_time;

  if (time_delta == 0) {  // Catch divide by zero
    time_delta = 1000;
  }

  display.clearDisplay();
  display.setCursor(0, 0);

  if (isRecording) {
    display.drawBitmap(0, -1, play_solid, 6, 8, DISPLAY_WHITE);
    display.setCursor(10, 0);
  }

  counts.add(new_total * 1000.0 / time_delta);

  const float avg_cps = counts.getAverage();
  const float avg_dt = dead_time.getAverage();
  float avg_cps_corrected = avg_cps;
  if (avg_dt > 0.) {
    avg_cps_corrected = avg_cps / (1.0 - avg_cps * avg_dt / 1.0e6);
  }

  display.print(avg_cps_corrected, 1);
  display.print(" cps");

  static int16_t temp = round(readTemp());

  if (!adc_lock) {  // Only update if ADC is free atm
    temp = round(readTemp());
  }

  display.setCursor(SCREEN_WIDTH - ((temp < 0) ? 36 : 30), 0);

  display.print(temp);
  display.print(" ");
  display.print((char)247);
  display.println("C");

  const unsigned long seconds_running = round((millis() - start_time) / 1000.0);
  const uint8_t char_offset = floor(log10(seconds_running));

  display.setCursor(SCREEN_WIDTH - 18 - char_offset * 6, 8);
  display.print(seconds_running);
  display.println(" s");

  for (uint16_t i = 0; i < SCREEN_WIDTH; i++) {
    const uint32_t val = round(eventBins[i] * scale_factor);
    display.drawFastVLine(i, SCREEN_HEIGHT - val - 1, val, DISPLAY_WHITE);
  }
  display.drawFastHLine(0, SCREEN_HEIGHT - 1, SCREEN_WIDTH, DISPLAY_WHITE);

  display.display();

  if (total > EVT_RESET_C) {
    clearSpectrumDisplay();
  }
}


void drawGeigerCounts() {
  uint32_t total = 0;

  for (uint16_t i = 0; i < ADC_BINS; i++) {
    total += display_spectrum[i];
  }

  const unsigned long now_time = millis();

  const uint32_t new_total = total - last_total;
  last_total = total;

  if (now_time < last_time) {  // Catch Millis() Rollover
    last_time = now_time;
    return;
  }

  unsigned long time_delta = now_time - last_time;
  last_time = now_time;

  if (time_delta == 0) {  // Catch divide by zero
    time_delta = 1000;
  }

  counts.add(new_total * 1000.0 / time_delta);

  const float avg_cps = counts.getAverage();
  const float avg_dt = dead_time.getAverage();  //+ 5.0;
  float avg_cps_corrected = avg_cps;
  if (avg_dt > 0.) {
    avg_cps_corrected = avg_cps / (1.0 - avg_cps * avg_dt / 1.0e6);
  }

  static float max_cps = -1;
  static float min_cps = -1;

  if (max_cps == -1 || avg_cps_corrected > max_cps) {
    max_cps = avg_cps_corrected;
  }
  if (min_cps <= 0 || avg_cps_corrected < min_cps) {
    min_cps = avg_cps_corrected;
  }

  display.clearDisplay();
  display.setCursor(0, 0);

  if (isRecording) {
    display.drawBitmap(SCREEN_WIDTH - 7, 9, play_solid, 6, 8, DISPLAY_WHITE);
  }


  display.print("Min: ");
  display.println(min_cps, 1);

  display.print("Max: ");
  display.println(max_cps, 1);

  static int16_t temp = round(readTemp());

  if (!adc_lock) {  // Only update if ADC is free atm
    temp = round(readTemp());
  }

  display.setCursor(SCREEN_WIDTH - ((temp < 0) ? 36 : 30), 0);

  display.print(temp);
  display.print(" ");
  display.print((char)247);
  display.println("C");

  display.setCursor(0, 0);
  display.setTextSize(2);

  display.drawFastHLine(0, 18, SCREEN_WIDTH, DISPLAY_WHITE);

  display.setCursor(0, 26);

  if (avg_cps_corrected > 1000) {
    display.print(avg_cps_corrected / 1000.0, 2);
    display.print("k");
  } else {
    display.print(avg_cps_corrected, 1);
  }

  display.setTextSize(1);
  display.println(" cps");

  display.display();

  if (total > EVT_RESET_C) {
    clearSpectrumDisplay();
  }
}
/*
  END DISPLAY FUNCTIONS
*/

void eventInt() {
  // Disable interrupt generation for this pin ASAP.
  // Directly uses Core0 IRQ Ctrl (core1 does not set the interrupt).
  // Thanks a lot to all the replies at
  // https://github.com/earlephilhower/arduino-pico/discussions/1397!
  // NOTE: NOT SUPPORTED ON PICO 2 / RP2350
  //static io_rw_32 *addr = &(iobank0_hw->proc0_irq_ctrl.inte[INT_PIN / 8]);
  //static uint32_t mask1 = 0b1111 << (INT_PIN % 8) * 4u;
  //hw_clear_bits(addr, mask1);

  const unsigned long start = micros();

  digitalWriteFast(LED, HIGH);  // Enable activity LED

  //const unsigned long start_millis = millis();
  //static unsigned long last_tick = start_millis;  // Last buzzer tick in ms, not needed with tone()
  static uint8_t count = 0;

  // Check if ticker is enabled, currently not "ticking" and also catch the millis() overflow
  if (conf.enable_ticker /* && (start_millis - last_tick > BUZZER_TICK || start_millis < last_tick)*/) {
    if (count >= conf.tick_rate - 1) {             // Only click at every 10th count
      tone(BUZZER_PIN, BUZZER_FREQ, BUZZER_TICK);  // Worse at higher cps
      //last_tick = start_millis;
      count = 0;
    } else {
      count++;
    }
  }

  uint16_t mean = 0;

  if (!conf.geiger_mode && !adc_lock) {
    uint32_t sum = 0;

    for (size_t i = 0; i < conf.meas_avg; i++) {
      sum += analogRead(AIN_PIN);
    }

    resetSampleHold();

    float avg = float(sum) / float(conf.meas_avg);
    ;  // Use median instead of average?

    if (current_baseline <= avg) {  // Catch negative numbers
      // Subtract DC bias from pulse avg and then convert float --> uint16_t ADC channel
      mean = round(avg - current_baseline);
    }
  }

  if ((conf.ser_output || conf.enable_display || isRecording)) {
    events[event_position] = mean;
    spectrum[mean] += 1;
    display_spectrum[mean] += 1;
    if (event_position >= EVENT_BUFFER - 1) {  // Increment if memory available, else overwrite array
      event_position = 0;
    } else {
      event_position++;
    }
  }

  if (conf.enable_trng) {
    static uint8_t trng_index = 0;  // Timestamp index for True Random Number Generator

    // Calculations for the TRNG
    trng_stamps[trng_index] = micros();

    if (trng_index < 2) {
      trng_index++;
    } else {
      // Catch micros() overflow
      if (trng_stamps[1] > trng_stamps[0] && trng_stamps[2] > trng_stamps[1]) {
        const uint32_t delta0 = trng_stamps[1] - trng_stamps[0];
        const uint32_t delta1 = trng_stamps[2] - trng_stamps[1];

        bitWrite(random_num, bit_index, (delta0 < delta1));

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

  digitalWriteFast(LED, LOW);  // Disable activity LED

  const unsigned long end = micros();

  if (end >= start) {  // Catch micros() overflow
    // Compute Detector Dead Time
    total_events++;
    dead_time.add(end - start);
  }


  // NOTE: NOT SUPPORTED ON PICO 2 / RP2350
  // Re-enable interrupts
  //static uint32_t mask2 = 0b0100 << (INT_PIN % 8) * 4u;
  //hw_set_bits(addr, mask2);

  // Clear all interrupts on the executing core
  //irq_clear(15);  // IRQ 15 = SIO_IRQ_PROC0
}

/*
  BEGIN SETUP FUNCTIONS
*/
void setup() {
  pinMode(AMP_PIN, INPUT);
  pinMode(INT_PIN, INPUT);
  pinMode(AIN_PIN, INPUT);
  pinMode(RST_PIN, OUTPUT_12MA);
  pinMode(LED, OUTPUT);

  //gpio_set_slew_rate(RST_PIN, GPIO_SLEW_RATE_SLOW);  // Slow slew rate to reduce EMI
  gpio_set_slew_rate(LED, GPIO_SLEW_RATE_SLOW);  // Slow slew rate to reduce EMI

  analogReadResolution(ADC_RES);

  resetSampleHold(5);  // Reset before enabling the interrupts to avoid jamming

  attachInterrupt(digitalPinToInterrupt(INT_PIN), eventInt, FALLING);

  start_time = millis();  // Spectrum pulse collection has started
}


void setup1() {
  rp2040.wdt_begin(5000);  // Enable hardware watchdog to check every 5s

  // Undervolt a bit to save power, pretty conservative value could be even lower probably
  vreg_set_voltage(VREG_VOLTAGE_1_00);

  // Disable "Power-Saving" power supply option.
  // -> does not actually significantly save power, but output is much less noisy in HIGH!
  // -> Also with PS_PIN LOW I have experiences high-pitched (~ 15 kHz range) coil whine!
  pinMode(PS_PIN, OUTPUT_4MA);
  gpio_set_slew_rate(PS_PIN, GPIO_SLEW_RATE_SLOW);  // Slow slew rate to reduce EMI
  // Not really sure what results in better spectra...
  // It doesn't change a whole lot, so might as well keep the energy saving mode?
  digitalWrite(PS_PIN, LOW);
  //digitalWrite(PS_PIN, HIGH);

  pinMode(GND_PIN, INPUT);
  pinMode(VSYS_MEAS, INPUT);
  pinMode(VBUS_MEAS, INPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  // Define an array of CommandInfo objects with function pointers and descriptions
  CommandInfo allCommands[] = {
    { getSpectrumData, "read spectrum", "Read the spectrum histogram collected since the last reset." },
    { readSettings, "read settings", "Read the current settings (file)." },
    { deviceInfo, "read info", "Read misc info about the firmware and state of the device." },
    { fsInfo, "read fs", "Read misc info about the used filesystem." },
    { readDirFS, "read dir", "Show contents of the data directory." },
    { readFileFS, "read file", "<filename> Print the contents of the file <filename> from the data directory." },
    { removeFileFS, "remove file", "<filename> Remove the file <filename> from the data directory." },
    { toggleBaseline, "set baseline", "<toggle> Automatically subtract the DC bias (baseline) from each signal." },
    { toggleTRNG, "set trng", "<toggle> Either 'on' or 'off' to toggle the true random number generator output." },
    { toggleDisplay, "set display", "<toggle> Either 'on' or 'off' to enable or force disable OLED support." },
    { setMode, "set mode", "<mode> Either 'geiger' or 'energy' to disable or enable energy measurements. Geiger mode only counts pulses, but is a lot faster." },
    { setSerialOutMode, "set out", "<mode> Either 'events', 'spectrum' or 'off'. 'events' prints events as they arrive, 'spectrum' prints the accumulated histogram." },
    { setMeasAveraging, "set averaging", "<number> Number of ADC averages for each energy measurement. Takes ints, minimum is 1." },
    { setTickerRate, "set tickrate", "<number> Rate at which the buzzer ticks, ticks once every <number> of pulses. Takes ints, minimum is 1." },
    { toggleTicker, "set ticker", "<toggle> Either 'on' or 'off' to enable or disable the onboard ticker." },
    { recordStart, "record start", "<time [min]> <filename> Start spectrum recording for duration <time> in minutes, (auto)save to <filename>." },
    { recordStop, "record stop", "Stop the recording." },
    { recordStatus, "record status", "Get the recording status." },
    { clearSpectrumData, "reset spectrum", "Reset the on-board spectrum histogram." },
    { resetSettings, "reset settings", "Reset all the settings/revert them back to default values." },
    { rebootNow, "reboot", "Reboot the device." }
  };

  // Get the number of allCommands
  const size_t numCommands = sizeof(allCommands) / sizeof(allCommands[0]);

  // Loop through allCommands and register commands with both shell instances
  for (size_t i = 0; i < numCommands; ++i) {
    shell.registerCommand(new ShellCommand(allCommands[i].function, allCommands[i].name, allCommands[i].description));
    shell1.registerCommand(new ShellCommand(allCommands[i].function, allCommands[i].name, allCommands[i].description));
  }

  // Starts FileSystem, autoformats if no FS is detected
  LittleFS.begin();
  conf = loadSettings();  // Read all the detector settings from flash

  saveSettings();        // Create settings file if none is present
  writeDebugFileBoot();  // Update power cycle count

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

  // Set up buzzer
  pinMode(BUZZER_PIN, OUTPUT_12MA);
  gpio_set_slew_rate(BUZZER_PIN, GPIO_SLEW_RATE_SLOW);  // Slow slew rate to reduce EMI
  digitalWrite(BUZZER_PIN, LOW);

  shell.begin(2000000);
  shell1.begin(2000000);

  println("Welcome to the Open Gamma Detector!");
  println("Firmware Version " + FW_VERSION);

  if (conf.enable_display) {
    bool begin = false;

#if (SCREEN_TYPE == SCREEN_SSD1306)
    begin = display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS);
#elif (SCREEN_TYPE == SCREEN_SH1106)
    begin = display.begin(SCREEN_ADDRESS, true);
#endif

    if (!begin) {
      println("Failed communication with the display. Maybe the I2C address is incorrect?", true);
      conf.enable_display = false;
    } else {
      display.setRotation(2);
      display.setTextSize(2);  // Draw 2X-scale text
      display.setTextColor(DISPLAY_WHITE);

      display.clearDisplay();

      display.println("Open Gamma Detector");
      display.println();
      display.setTextSize(1);
      display.print("FW ");
      display.println(FW_VERSION);
      display.drawBitmap(128 - 34, 64 - 34, opengamma_pcb, 32, 32, DISPLAY_WHITE);

      display.display();
      //delay(2000);
    }
  }

  // Set up task scheduler and enable tasks
  updateDisplayTask.setSchedulingOption(TASK_INTERVAL);  // TASK_SCHEDULE, TASK_SCHEDULE_NC, TASK_INTERVAL
  dataOutputTask.setSchedulingOption(TASK_INTERVAL);
  queryButtonTask.setSchedulingOption(TASK_INTERVAL);
  //resetPHCircuitTask.setSchedulingOption(TASK_INTERVAL);
  //updateBaselineTask.setSchedulingOption(TASK_INTERVAL);

  schedule.init();
  schedule.allowSleep(true);

  schedule.addTask(writeDebugFileTimeTask);
  schedule.addTask(queryButtonTask);
  schedule.addTask(updateDisplayTask);
  schedule.addTask(dataOutputTask);
  schedule.addTask(updateBaselineTask);
  schedule.addTask(resetPHCircuitTask);
  schedule.addTask(recordCycleTask);

  queryButtonTask.enable();
  resetPHCircuitTask.enable();
  updateBaselineTask.enable();
  writeDebugFileTimeTask.enableDelayed(60 * 60 * 1000);
  dataOutputTask.enableDelayed(OUT_REFRESH);

  if (conf.enable_display) {
    // Only enable display task if the display function is enabled
    updateDisplayTask.enableDelayed(DISPLAY_REFRESH);
  }
}
/*
  END SETUP FUNCTIONS
*/

/*
  BEGIN LOOP FUNCTIONS
*/
void loop() {
  // Do nothing here

  __wfi();  // Wait for interrupt
}


void loop1() {
  schedule.execute();

  delay(1);  // Wait for 1 ms, slightly reduces power consumption
}
/*
  END LOOP FUNCTIONS
*/
