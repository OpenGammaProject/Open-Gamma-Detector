/*

  Misc helper functions used in the main Open Gamma Detector sketch.

  2023, NuclearPhoenix. Open Gamma Project.
  https://github.com/OpenGammaProject/Open-Gamma-Detector

*/

#include "Helper.h"

void cleanPrintln(const String &text) {
  if (Serial) {
    Serial.println(text);
  }
  if (Serial2) {
    Serial2.println(text);
  }
}


void cleanPrintln(unsigned int number, int base) {
  if (Serial) {
    Serial.println(number, base);
  }
  if (Serial2) {
    Serial2.println(number, base);
  }
}


void cleanPrint(const String &text) {
  if (Serial) {
    Serial.print(text);
  }
  if (Serial2) {
    Serial2.print(text);
  }
}


void cleanPrint(unsigned int number, int base) {
  if (Serial) {
    Serial.print(number, base);
  }
  if (Serial2) {
    Serial2.print(number, base);
  }
}


void print(String text, bool error) {
  text = (error ? "[!] " : "[#] ") + text;

  if (Serial) {
    Serial.print(text);
  }
  if (Serial2) {
    Serial2.print(text);
  }
}


void print(unsigned int number, bool error) {
  print(String(number), error);
}


void println(String text, bool error) {
  text = (error ? "[!] " : "[#] ") + text;

  if (Serial) {
    Serial.println(text);
  }
  if (Serial2) {
    Serial2.println(text);
  }
}


void println(unsigned int number, bool error) {
  println(String(number), error);
}
