/*

  Helper functions for all actions related solely to the file system.
  Used in the main Open Gamma Detector sketch.

  2024, NuclearPhoenix. Open Gamma Project.
  https://github.com/OpenGammaProject/Open-Gamma-Detector

*/

#include "FS.h"

#include <LittleFS.h>  // Needed to interface with the FS on flash

uint8_t getUsedPercentageFS() {
  FSInfo fsinfo;
  LittleFS.info(fsinfo);

  return round(float(fsinfo.usedBytes) / fsinfo.totalBytes * 100.0);
}


void fsInfo([[maybe_unused]] String *args) {
  FSInfo fsinfo;
  LittleFS.info(fsinfo);

  Dir dir = LittleFS.openDir(DATA_DIR_PATH);
  size_t dirSize = 0;
  while (dir.next()) {
    dirSize += dir.fileSize();
  }

  println("Total Size: \t" + String(fsinfo.totalBytes / 1000.0) + " kB");
  print("Used Size: \t\t" + String(fsinfo.usedBytes / 1000.0) + " kB");
  cleanPrintln(" / " + String(getUsedPercentageFS()) + " %");
  println("Data Dir Size: \t" + String(dirSize / 1000.0) + " kB");
  // Uncomment these for advanced users
  //println("Block Size: " + String(fsinfo.blockSize / 1000.0) + " kB");
  //println("Page Size: " + String(fsinfo.pageSize) + " B");
  //println("Max Open Files: " + String(fsinfo.maxOpenFiles));
  //println("Max Path Length: " + String(fsinfo.maxPathLength));
}


void readDirFS([[maybe_unused]] String *args) {
  Dir dir = LittleFS.openDir(DATA_DIR_PATH);
  size_t counter = 0;

  while (dir.next()) {
    print(dir.fileName() + ": \t");
    cleanPrintln(String(dir.fileSize() / 1000.0) + " kB");
    counter++;
  }

  if (counter == 0) {
    println("Directory is empty.");
  }
}


void readFileFS(String *args) {
  String command = *args;
  command.replace("read file", "");
  command.trim();

  if (command == "") {
    println("No inpute file given! You must supply a file name.", true);
    return;
  }

  if (!LittleFS.exists(DATA_DIR_PATH + command)) {
    println("Could not open file. File '" + command + "' does not exist!", true);
    return;
  }

  File f = LittleFS.open(DATA_DIR_PATH + command, "r");

  if (!f) {
    println("Could not open file'" + command + "'!", true);
    return;
  }

  while (f.available()) {
    cleanPrintln(f.readString());
  }

  f.close();
}


void removeFileFS(String *args) {
  String command = *args;
  command.replace("remove file", "");
  command.trim();

  if (command == "") {
    println("No inpute file given! You must supply a file name.", true);
    return;
  }

  if (!LittleFS.exists(DATA_DIR_PATH + command)) {
    println("Could not delete file. File '" + command + "' does not exist!", true);
    return;
  }

  if (LittleFS.remove(DATA_DIR_PATH + command)) {
    println("Successfully deleted '" + command + "'.");
  } else {
    println("Could not delete file '" + command + "'!", true);
  }
}
