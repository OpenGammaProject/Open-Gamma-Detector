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

### Arduino Board

Before we can flash the MCU we have to do the Arduino IDE setup. Because I'm using the Adafruit Itsy Bitsy M4 there are some steps we have to go through
in order for this to work properly. Fortunately, Adafruit already has a great [tutorial](https://learn.adafruit.com/introducing-adafruit-itsybitsy-m4/setup)
on how to do this.

After you've completed all the steps successfully you can go ahead and download the `opensc.ino` located in the [opensc folder](software/microcontroller/) and open
the ino file. You might be prompted to create a folder for this sketch, just say yes.

First of all, in your boards manager select the Adafruit ItsyBitsy M4, the default options are ok here, you might want to set `optimize` to something fast like -O3 or -Ofast - we've got plenty of space anyway. You will also need the following libraries, just download those from the lib manager:

* [Adafruit SSD1306](https://github.com/adafruit/Adafruit_SSD1306)
* [Adafruit DotStar](https://github.com/adafruit/Adafruit_DotStar)
* [Adafruit SPIFlash](https://github.com/adafruit/Adafruit_SPIFlash)

Now you're ready to flash the sketch!

_ToDo: Software Calibration!_

_ToDo: Stand-alone measurements and readout w/o serial connection_

### PC Software

Now to get all the data from your MCU to your PC there are a couple of python scripts that will deal with the serial input. These can be found in `software/pc/`, 
please download the whole directory and cd to it. You will need the latest version of Python 3 and a couple of packages.
If you're on Windows you can get python via the Windows Store, if you're on any other OS you'll figure it out pretty easily.
To get all the required packages, open a terminal window and type in the following command:

```bash
pip3 install -r requirements.txt
```

Here is a short description of the two main python scripts:

#### serial_data.py

This will take the serial data in, output some live info as well as an optional live histogram and print the data to a csv file.
There are essentially three settings for you to change:

```text
filename = "out.csv"  --> That's where all the data will be printed to
plot = True           --> When true, there will be a live plot
refresh_rate = 1 #Hz  --> Refresh rate for the live plot
```

Please also check the line `ser = serial.Serial("COM3", 2000000)` for the correct serial port.
The csv file will always start with the following header:

```text
### Begin OpenSC Data ###

timestamp in seconds;timestamp in nanoseconds

```

All the data will be formatted like this:

```text
timestamp in nanoseconds;nanoseconds since the last event;event amplitude (V)
```

#### hist_processor.py

This will take the output file from serial_data.py and convert the data to make it really easy to plot a histogram.
All the event voltages from 0.000 to 3.300 will be counted with a resolution of 0.001V. The number of events per mV will then be printed in ascending order (so from 0 to 3.3) to the output file which will be `YOURINPUTFILENAME-p.csv`.

There are only two things for you to change here:

```text
filename = "out.csv"  --> The input file name
RELATIVE = False      --> If this is true each number of events (again, per mV) will be divided by the total number of events
```

After you created your histogram data with this script you can go into MS Office/LibreOffice, import the csv, select all the data and e.g. do a line chart or bar chart or whatever. Obviously do not use the built-in histogram function with this. You can, however, directly import the serial output csv and do a histogram if your software of choice supports it.

**The last file (`data_plot_manual.py`) just takes the serial data as an input and plots it.**

## Example measurements

_To Do: Will add some example measurements of Am241, some uranium glaze, radium, pitchblende ..._

## Limitations and Ideas

_Will contain: (Water-) Cooling SiPM? Shielding background radiation. Trying to do some gamma spectroscopy? Dual opamp stage for the pre-amp? This is where the fun begins._
