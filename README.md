# Open Scintillation Counter

Open hardware for a simple all-in-one scintillation counter design using a popular NaI(Tl) scintillation crystal.

Hardware design has been done in EasyEDA and all the needed files for you to import the project as well as
a schematic can be found in the hardware folder. There is also a gerber file available for direct pcb manufacturing.

Software aims to be as simple as possible to understand and maintain; to achieve this I decided to use an off-the-shelf
microntroller - the Adafruit ItsyBitsy M4 Express. This board can be programmed over micro USB and is powerful (fast)
enough for the purpose.

## Hardware

This project utilizes a silicon photomultiplier (short SiPM) which is way more robust than a traditional photomultiplier tube and does not need any high voltage supply. I'll link some in-depth datasheets about this particular SiPM:

* [C-Series SiPM Sensors datasheet](https://www.mouser.at/datasheet/2/308/MICROC-SERIES-D-1489614.pdf)
* [Biasing and Readout of ON Semiconductor SiPM Sensors](https://www.onsemi.com/pub/Collateral/AND9782-D.PDF)
* [Introduction to the SiliconPhotomultiplier (SiPM)](https://www.onsemi.com/pub/Collateral/AND9770-D.PDF)

This project contains of essentially two PCBs, one for the SiPM (`sipm` folder) that will be mounted on top of your scintillation crystal.
It will then be connected to the other part: the evaluation hardware and counter (`detector` folder).

Attention when mounting the SiPM to the PCB: One of the four legs in each corner is slightly bigger than the rest, this one must be lined up with the dot on the pcb!

After you soldered all the components to the SiPM PCB you can mount it to your crystal. For that I am using a small (18x30mm) standard NaI(Tl) crystal.
These can be bought cheaply from ebay for example. Most of the cheap sellers are from Russia, Ukraine or similar.
However, there are many more types of scintillation materials that you can use. Just be sure it's sensitive to gamma radiation and it's peak emission wavelength
is near the peak sensitivity of the SiPM which is at about 420nm.

You'll also want to use something to optically couple the crystal with the SiPM in order to minimize reflection and therefore increase photon detection rates.
I'm using some standard silicone grease; just apply a thin layer, then press the two parts together and wrap them with a couple of layers of electrical tape so that
no light can get in.

After that just connect the three jumper wires to the MCU board according to the schematics and you're ready to go.

__The detector hardware needs a slight re-design. Ideally most of the hardware can be mounted on one small PCB on top of the crystal.__

## Software



_To Do: What Arduino sketches are there, what libraries and board definitions are needed, how to program,
PC software: different scripts, installation, how to use, data output._

## Example measurements

_To Do: Will add some example measurements of Am241, some uranium glaze, radium, pitchblende ..._

## Limitations and Ideas

_Will contain: (Water-) Cooling SiPM? Shielding background radiation. Trying to do some gamma spectroscopy? Dual opamp stage for the pre-amp? This is where the fun begins._
