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
   TODO: Sleep modes instead of delays
   TODO: Add more OLED features?
   TODO: Print spectrum only? (i.e. remove EVENT_BUFFER)

*/

#include <hardware/adc.h>  // For corrected temp readings

#include <SimpleShell_Enhanced.h>  // Serial Commands/Console
#include <ArduinoJson.h>           // Load and save the settings file
#include <LittleFS.h>              // Used for FS, stores the settings file
#include <Adafruit_SSD1306.h>      // Used for OLEDs

const String FWVERS = "2.5.1";  // Firmware Version Code

const uint8_t GND_PIN = A0;    // GND meas pin
const uint8_t VCC_PIN = A2;    // VCC meas pin
const uint8_t VSYS_MEAS = A3;  // VSYS/3
const uint8_t VBUS_MEAS = 24;  // VBUS Sense Pin
const uint8_t PS_PIN = 23;     // SMPS power save pin

const uint8_t AIN_PIN = A1;         // Analog input pin
const uint8_t INT_PIN = 16;         // Signal interrupt pin
const uint8_t RST_PIN = 22;         // Peak detector MOSFET reset pin
const uint8_t LED = 25;             // LED on GP25
const uint16_t EVT_RESET_C = 2000;  // Number of counts after which the OLED stats will be reset

/*
    BEGIN USER SETTINGS
*/
// These are the default settings that can only be changed by reflashing the Pico
const float VREF_VOLTAGE = 3.3;        // ADC reference voltage, defaults 3.3, with reference 3.0
const uint8_t ADC_RES = 12;            // Use 12-bit ADC resolution
const uint32_t EVENT_BUFFER = 100000;  // Buffer this many events for Serial.print
const uint8_t SCREEN_WIDTH = 128;      // OLED display width, in pixels
const uint8_t SCREEN_HEIGHT = 64;      // OLED display height, in pixels
const uint8_t SCREEN_ADDRESS = 0x3C;   // See datasheet for Address; 0x3D for 128x64, 0x3C for 128x32
const uint8_t TRNG_BITS = 8;           // Number of bits for each random number, max 8

struct Config {
  // These are the default settings that can also be changes via the serial commands
  volatile bool ser_output = true;       // Wheter data should be Serial.println'ed
  volatile bool geiger_mode = false;     // Measure only cps, not energy
  volatile bool print_spectrum = false;  // Print the finishes spectrum, not just
  volatile bool auto_reset = true;       // Periodically reset S&H circuit
  volatile size_t meas_avg = 5;          // Number of meas. averaged each event, higher=longer dead time
  volatile bool enable_display = false;  // Enable I2C Display, see settings above
  volatile bool trng_enabled = false;    // Enable the True Random Number Generator
  volatile size_t delay_time = 1;        // Time between Trigger and ADC readout of pulses
};
/*
   END USER SETTINGS
*/

volatile uint32_t spectrum[uint16_t(pow(2, ADC_RES))];  // Holds the spectrum histogram written to flash
volatile uint16_t events[EVENT_BUFFER];                 // Buffer array for single events
volatile uint16_t event_position = 0;                   // Target index in events array
volatile uint32_t last_time = 0;                        // Last timestamp for OLED refresh

volatile uint32_t trng_stamps[3];          // Timestamps for True Random Number Generator
volatile uint8_t trng_index = 0;           // Timestamp index for True Random Number Generator
volatile uint8_t random_num = 0b00000000;  // Generated random bits that form a byte together
volatile uint8_t bit_index = 0;            // Bit index for the generated number
volatile uint32_t trng_nums[1000];         // TRNG number output array
volatile uint16_t number_index = 0;        // Amount of saved numbers to the TRNG array

volatile float deadtime_avg = 0;   // Average detector dead time in µs
volatile uint32_t dt_avg_num = 0;  // Number of dead time measurements

volatile Config conf;  // Configuration object
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);


void serialInterruptMode(String *args) {
  String command = *args;
  command.replace("set int", "");
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
  command.replace("set reset", "");
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


void setDisplay(String *args) {
  String command = *args;
  command.replace("set display", "");
  command.trim();

  if (command == "enable") {
    conf.enable_display = true;
    Serial.println("Enabled display output. You might need to reset the device.");
  } else if (command == "disable") {
    conf.enable_display = false;
    Serial.println("Disabled display output. You might need to reset the device.");
  } else {
    Serial.println("No valid input '" + command + "'!");
    Serial.println("Must be 'enable' or 'disable'.");
    return;
  }
  saveSetting();
}


void toggleGeigerMode(String *args) {
  String command = *args;
  command.replace("set mode", "");
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


void toggleTRNG(String *args) {
  String command = *args;
  command.replace("set trng", "");
  command.trim();

  if (command == "enable") {
    conf.trng_enabled = true;
    Serial.println("Enabled True Random Number Generator output.");
  } else if (command == "disable") {
    conf.trng_enabled = false;
    Serial.println("Disabled True Random Number Generator output.");
  } else {
    Serial.println("No valid input '" + command + "'!");
    Serial.println("Must be 'enable' or 'disable'.");
    return;
  }
  saveSetting();
}


void measAveraging(String *args) {
  String command = *args;
  command.replace("set averaging", "");
  command.trim();
  const long number = command.toInt();

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


void delayTime(String *args) {
  String command = *args;
  command.replace("set delay", "");
  command.trim();
  const long number = command.toInt();

  if (number > 0) {
    conf.delay_time = number;
    Serial.println("Set delay time to " + String(number) + " µs.");
  } else {
    Serial.println("No valid input '" + command + "'!");
    Serial.println("Parameter must be a number >= 0.");
    return;
  }
  saveSetting();
}


float analogReadTempCorrect() {
  digitalWrite(RST_PIN, HIGH);  // ADDED: Disable peak&hold

  // Copy from arduino-pico/cores/rp2040/wiring_analog.cpp
  adc_init();  // Init ADC just to be sure
  adc_set_temp_sensor_enabled(true);

  //digitalWrite(PS_PIN, HIGH); // Disable Power Save For Better Noise
  //detachInterrupt(digitalPinToInterrupt(INT_PIN)); // ADDED: Disable interrupts shortly

  delay(1);  // Allow things to settle.  Without this, readings can be erratic
  adc_select_input(4);
  const uint16_t v = adc_read();

  //digitalWrite(PS_PIN, LOW); // Re-Enable Power Saving
  //resetSampleHold(); // ADDED: Reset S&H after the detached interrupt and re-enable interrupts
  //attachInterrupt(digitalPinToInterrupt(INT_PIN), eventInt, HIGH);

  adc_set_temp_sensor_enabled(false);

  digitalWrite(RST_PIN, LOW);  // ADDED: Enable peak&hold again

  return 27.0 - ((v * VREF_VOLTAGE / (pow(2, ADC_RES) - 1)) - 0.706) / 0.001721;
}


void deviceInfo([[maybe_unused]] String *args) {
  Serial.println("=========================");
  Serial.println("-- Open Gamma Detector --");
  Serial.println("By NuclearPhoenix, Open Gamma Project");
  Serial.println("2022. https://github.com/Open-Gamma-Project");
  Serial.println("Firmware Version: " + FWVERS);
  Serial.println("=========================");
  Serial.println("Runtime: " + String(millis() / 1000.0) + " s");
  Serial.println("Avg. dead time: " + String(deadtime_avg) + " µs");

  const float deadtime_frac = float(deadtime_avg * dt_avg_num) / 1000.0 / float(millis()) * 100.0;

  Serial.println("Total dead time: " + String(deadtime_frac) + " %");
  Serial.println("CPU Frequency: " + String(rp2040.f_cpu() / 1e6) + " MHz");
  Serial.println("Used Heap Memory: " + String(rp2040.getUsedHeap() / 1000.0) + " kB");
  Serial.println("Total Heap Size: " + String(rp2040.getTotalHeap() / 1000.0) + " kB");
  Serial.println("Temperature: " + String(round(analogReadTempCorrect())) + " °C");
  Serial.println("USB Connection: " + String(bool(digitalRead(VBUS_MEAS))));

  const float v = 3.0 * analogRead(VSYS_MEAS) * VREF_VOLTAGE / (pow(2, ADC_RES) - 1);

  Serial.println("Supply Voltage: " + String(round(v * 10.0) / 10.0) + " V");
}


void fsInfo([[maybe_unused]] String *args) {
  FSInfo fsinfo;
  LittleFS.info(fsinfo);
  Serial.println("Total Size: " + String(fsinfo.totalBytes / 1000.0) + " kB");
  Serial.print("Used Size: " + String(fsinfo.usedBytes / 1000.0) + " kB");
  Serial.println(" / " + String(float(fsinfo.usedBytes) / fsinfo.totalBytes * 100) + " %");
  Serial.println("Block Size: " + String(fsinfo.blockSize / 1000.0) + " kB");
  Serial.println("Page Size: " + String(fsinfo.pageSize) + " B");
  Serial.println("Max Open Files: " + String(fsinfo.maxOpenFiles));
  Serial.println("Max Path Length: " + String(fsinfo.maxPathLength));
}


void getSpectrumData([[maybe_unused]] String *args) {
  for (size_t i = 0; i < pow(2, ADC_RES); i++) {
    Serial.println(spectrum[i]);
  }
}


void clearSpectrum() {
  for (size_t i = 0; i < pow(2, ADC_RES); i++) {
    spectrum[i] = 0;
  }
}


void clearSpectrumData([[maybe_unused]] String *args) {
  Serial.println("Clearing spectrum...");
  last_time = millis();
  clearSpectrum();
  Serial.println("Cleared!");
}


void readSettings([[maybe_unused]] String *args) {
  File saveFile = LittleFS.open("/config.json", "r");

  if (!saveFile) {
    Serial.println("Could not open save file!");
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
    Serial.print("Could not load config from json file: ");
    Serial.println(error.f_str());
    return;
  }

  serializeJsonPretty(doc, Serial);

  Serial.println();
  Serial.println("Read settings file successfully.");
}


void resetSettings([[maybe_unused]] String *args) {
  const Config defaultConf;
  conf.ser_output = defaultConf.ser_output;
  conf.geiger_mode = defaultConf.geiger_mode;
  conf.print_spectrum = defaultConf.print_spectrum;
  conf.auto_reset = defaultConf.auto_reset;
  conf.meas_avg = defaultConf.meas_avg;
  conf.enable_display = defaultConf.enable_display;
  conf.trng_enabled = defaultConf.trng_enabled;
  conf.delay_time = defaultConf.delay_time;
  Serial.println("Applied default settings.");

  saveSetting();
}


void rebootNow([[maybe_unused]] String *args) {
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

  DynamicJsonDocument doc(1024);

  doc["ser_output"] = conf.ser_output;
  doc["geiger_mode"] = conf.geiger_mode;
  doc["print_spectrum"] = conf.print_spectrum;
  doc["auto_reset"] = conf.auto_reset;
  doc["meas_avg"] = conf.meas_avg;
  doc["enable_display"] = conf.enable_display;
  doc["trng_enabled"] = conf.trng_enabled;
  doc["delay_time"] = conf.delay_time;

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
  conf.enable_display = doc["enable_display"];
  conf.trng_enabled = doc["trng_enabled"];
  conf.delay_time = doc["delay_time"];

  Serial.println("Successfuly loaded config from flash.");
}


void resetSampleHold() {  // Reset sample and hold circuit
  digitalWrite(RST_PIN, HIGH);
  delayMicroseconds(1);  // Discharge for 1 µs, actually takes 2 µs - enough for a discharge
  digitalWrite(RST_PIN, LOW);
}


void drawSpectrum() {
  const uint16_t BINSIZE = round(pow(2, ADC_RES) / SCREEN_WIDTH);
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

  if (max_num <= 0) {  // No events accumulated, catch divide by zero
    return;
  }

  const float scale_factor = float(SCREEN_HEIGHT - 11) / float(max_num);
  const uint32_t time_delta = millis() - last_time;

  if (time_delta <= 0) {  // Catch divide by zero
    return;
  }

  display.clearDisplay();
  display.setCursor(0, 0);

  display.print(total * 1000.0 / time_delta);
  display.print(" cps");

  const int16_t temp = round(analogReadTempCorrect());

  if (temp < 0) {
    display.setCursor(SCREEN_WIDTH - 30, 0);
  } else {
    display.setCursor(SCREEN_WIDTH - 24, 0);
  }
  display.print(temp);
  display.println(" C");

  const uint32_t seconds_running = round(time_delta / 1000.0);

  if (seconds_running < 10) {
    display.setCursor(SCREEN_WIDTH - 18, 8);
  } else if (seconds_running < 100) {
    display.setCursor(SCREEN_WIDTH - 24, 8);
  } else if (seconds_running < 1000) {
    display.setCursor(SCREEN_WIDTH - 30, 8);
  } else {
    display.setCursor(SCREEN_WIDTH - 36, 8);
  }
  display.print(seconds_running);
  display.println(" s");

  for (uint16_t i = 0; i < SCREEN_WIDTH; i++) {
    uint16_t val = round(eventBins[i] * scale_factor);
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
  delayMicroseconds(conf.delay_time);  // Wait to allow the sample/hold circuit to stabilize

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
      avg = 0.0;
    } else {
      avg /= conf.meas_avg - invalid;
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
  }

  if (conf.ser_output || conf.enable_display) {
    events[event_position] = mean;
    spectrum[mean] += 1;
    //Serial.print(' ' + String(sqrt(var)) + ';');
    //Serial.println(' ' + String(sqrt(var)/mean) + ';');
    if (event_position >= EVENT_BUFFER - 1) {  // Increment if memory available, else overwrite array
      event_position = 0;
    } else {
      event_position++;
    }
  }

  resetSampleHold();

  if (conf.trng_enabled) {
    // Calculations for the TRNG
    // TO FIX: Zero is overrepresented!
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

  Shell.registerCommand(new ShellCommand(getSpectrumData, "read spectrum", "Read the spectrum histogram collected since the last reset."));
  Shell.registerCommand(new ShellCommand(readSettings, "read settings", "Read the current settings (file)."));
  Shell.registerCommand(new ShellCommand(deviceInfo, "read info", "Read misc info about the firmware and state of the device."));
  Shell.registerCommand(new ShellCommand(fsInfo, "read fs", "Read misc info about the used filesystem."));

  Shell.registerCommand(new ShellCommand(toggleTRNG, "set trng", "<toggle> Either 'enable' or 'disable' to enable/disable the true random number generator output."));
  Shell.registerCommand(new ShellCommand(setDisplay, "set display", "<toggle> Either 'enable' or 'disable' to enable or force disable OLED support."));
  Shell.registerCommand(new ShellCommand(toggleGeigerMode, "set mode", "<mode> Either 'geiger' or 'energy' to disable or enable energy measurements. Geiger mode only counts cps, but has ~3x higher saturation."));
  Shell.registerCommand(new ShellCommand(serialInterruptMode, "set int", "<mode> Either 'events', 'spectrum' or 'disable'. 'events' prints events as they arrive, 'spectrum' prints the accumulated histogram."));
  Shell.registerCommand(new ShellCommand(toggleAutoReset, "set reset", "<toggle> Either 'enable' or 'disable' for periodic resets of the P&H circuit. Helps with mains interference to the cap, but adds ~4 ms dead time."));
  Shell.registerCommand(new ShellCommand(measAveraging, "set averaging", "<number> Number of ADC averages for each energy measurement. Takes ints, minimum is 1."));
  Shell.registerCommand(new ShellCommand(delayTime, "set delay", "<number> Delay between trigger and ADC readout of pulses in µs. Set this to ~1/2 of the maximum pulse duration you are expecting. Minimum is 1."));

  Shell.registerCommand(new ShellCommand(clearSpectrumData, "clear spectrum", "Clear the on-board spectrum hist."));
  Shell.registerCommand(new ShellCommand(resetSettings, "clear settings", "Clear all the settings and revert them back to default values."));
  Shell.registerCommand(new ShellCommand(rebootNow, "reboot", "Reboot the device."));

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

  if (conf.enable_display) {
    if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
      Serial.println("Failed communication with the display. Maybe the I2C address is incorrect?");
      conf.enable_display = false;
    } else {
      display.setTextSize(1);  // Draw 2X-scale text
      display.setTextColor(SSD1306_WHITE);

      display.clearDisplay();
      display.println("Open Gamma Detector");
      display.print("FW ");
      display.println(FWVERS);
      display.display();
      delay(1000);
    }
  }
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

  rp2040.wdt_reset();  // Reset watchdog, everything is fine

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
      } else if (event_position > 0 && event_position <= EVENT_BUFFER) {
        for (uint16_t index = 0; index < event_position; index++) {
          Serial.print(events[index]);
          Serial.print(";");
        }
        Serial.println();
      }
    }

    event_position = 0;
  }

  if (conf.trng_enabled) {
    if (Serial) {
      for (size_t i = 0; i < number_index; i++) {
        Serial.print(trng_nums[i], DEC);
        Serial.println(";");
      }
      number_index = 0;
    }
  }

  if (conf.enable_display) {
    drawSpectrum();
  }

  delay(1000);  // Wait for 1 sec, better: sleep for power saving?!
}