/*

  Helper functions for all actions related solely to the file system.
  Used in the main Open Gamma Detector sketch.

  2024, NuclearPhoenix. Open Gamma Project.
  https://github.com/OpenGammaProject/Open-Gamma-Detector

*/

#ifndef FS_H
#define FS_H

#include "Helper.h"  // Misc helper functions (Serial, data types, ...)

const String DATA_DIR_PATH = "/data/";

uint8_t getUsedPercentageFS();

void fsInfo(String *args);
void readDirFS(String *args);
void readFileFS(String *args);
void removeFileFS(String *args);

#endif  // FS_H
