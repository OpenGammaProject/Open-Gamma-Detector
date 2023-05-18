/*

  Misc helper functions used for the Open Gamma Detector sketch.

  2023, NuclearPhoenix. Open Gamma Project.
  https://github.com/OpenGammaProject/Open-Gamma-Detector

*/

#include "Arduino.h"

// Display Types
#define SCREEN_SH1106 0
#define SCREEN_SSD1306 1

void cleanPrintln(const String &text = String(""));
void cleanPrint(const String &text = String(""));
void cleanPrintln(unsigned int number, int base = DEC);
void cleanPrint(unsigned int number, int base = DEC);

void print(String text = "", bool error = false);
void println(String text = "", bool error = false);
