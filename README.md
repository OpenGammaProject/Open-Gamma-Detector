# Open Scintillation Counter

Open hardware for a simple all-in-one scintillation counter design using a popular NaI(Tl) scintillation crystal.

Hardware design has been done in EasyEDA and all the needed files for you to import the project as well as
a schematic can be found in the hardware folder. There is also a gerber file available for direct pcb manufacturing.

Software aims to be as simple as possible to understand and maintain; to achieve this I decided to use an off-the-shelf
microntroller - the Adafruit ItsyBitsy M4 Express. This board can be programmed over micro USB and is powerful (fast)
enough for the purpose.

## Hardware

This project contains of essentially two PCBs, one for the SiPM (`sipm` folder) that will be mounted on top of your scintillation crystal.
It will then be connected to the other part: the evaluation hardware and counter (`detector` folder).

_To Do: Explain how to mount SiPM and connect it. What crystals to use?_

__All the detector hardware needs a re-design. Ideally all the hardware can be mounted on one small PCB on top of the crystal.__

## Software

_To Do: What Arduino sketches are there, what libraries and board definitions are needed, how to program,
PC software: different scripts, installation, how to use, data output._

## Example measurements

_To Do: Will add some example measurements of Am241, some uranium glaze, radium, pitchblende ..._

## Limitations and Ideas

_Will contain: (Water-) Cooling SiPM? Trying to do some gamma spectroscopy? Dual opamp stage for the pre-amp? This is where the fun begins._
