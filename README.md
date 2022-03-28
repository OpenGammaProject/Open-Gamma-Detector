# Open Gamma Detector

Open hardware for a simple, yet powerful, all-in-one scintillation counter design using a popular NaI(Tl) scintillation crystal. Can be used for gamma spectroscopy while being significantly cheaper than any off-the-shelf platform.

Hardware design has been done with [EasyEDA](https://easyeda.com/) and all the needed files for you to import the project as well as the schematics can be found in the `electronics` folder. There are also Gerber files available for you to go directly to the PCB manufacturing step.

The software aims to be as simple as possible to understand and maintain; to achieve this I decided to use an off-the-shelf microcontroller - the [Raspberry Pi Pico](https://www.raspberrypi.com/products/raspberry-pi-pico/). This board can be programmed with the Arduino IDE over micro USB and is powerful (dual core, good ADC, plenty of memory, ...) enough for the purpose and also exceptionally cheap.

## Working Principle

<p align="center">
  <img alt="Working Principle Flow Chart" title="Working Principle Flow Chart" src="docs/flow.drawio.png">
</p>

## Key Features

Here are some of the most important key facts:

* Compact design: Only 60 x 60 mm (not including scintillator).
* All-in-one detector: No external sound card required.
* Micro-USB serial connection and power.
* Easily programmable using the Arduino IDE.
* Low-voltage device: No HV needed for PMT.
* Low power consumption: ~25 mA @ 5V.
* Capable of up to 10,000 counts per second with a serial connection.
* 4096 ADC channels for the energy range of about 30 keV to 1300 keV.

## Hardware

This project utilizes a silicon photomultiplier (short SiPM) which is way smaller and more robust than a traditional photomultiplier tube and does not need a high-voltage supply. Here are some very helpful in-depth datasheets about this particular SiPM:

* [C-Series SiPM Sensors datasheet](https://www.onsemi.com/pdf/datasheet/microc-series-d.pdf)
* [Linearity of the Silicon Photomultiplier](https://www.onsemi.com/pub/Collateral/AND9776-D.PDF)
* [Introduction to the SiliconPhotomultiplier (SiPM)](https://www.onsemi.com/pub/Collateral/AND9770-D.PDF)
* [Biasing and Readout of ON Semiconductor SiPM Sensors](https://www.onsemi.com/pub/Collateral/AND9782-D.PDF)

The hardware consists of two PCBs: one carrier board for the SiPM (`electronics/sipm` folder) that will be mounted on top of your scintillation crystal. It will then be connected to the other board: the main detector (`electronics/detector` folder) which includes amplification, pulse detection and energy measurement. If you already have a SiPM/crystal assembly you may use it with the detector board by leaving the pin header out and soldering wires directly to the pads.

The heart of the detector board is the Raspberry Pi Pico which uses its (calibrated) ADC to measure the pulse amplitude (e.g. the energy) immediately after an event occurs using an interrupt. I can really recommend you reading the datasheet or maybe also having a look at a deeper analysis of the Pico ADC:

* [Raspberry Pi Pico](https://datasheets.raspberrypi.com/pico/pico-datasheet.pdf)
* [Characterizing the Raspberry Pi Pico ADC](https://pico-adc.markomo.me/)

Here are some renders of the detector PCB. Size is roughly 6x6 cm.

<p align="center">
  <img alt="Front Side PCB" title="Front Side PCB" src="docs/pcb_front.png" style="width:49%">
  <img alt="Back Side PCB" title="Back Side PCB" src="docs/pcb_back.png" style="width:49%">
</p>

### Scintillator Assembly

The finished SiPM carrier board is there to allow for easier packaging with the scintillator as well as to be reusable for different detectors as that's by far the most expensive part and you'll want to use it as long as possible. You need to apply some optical coupling compound between the SiPM and the crystal window to reduce reflections as good as possible (this way the best photon detection efficiency is achieved). There are also special materials for this usecase but you can use some standard silicone grease - works great for me. After you applied some, you press both parts together and wrap everything with light-tight tape, again, I'm just using some black electrical tape here. That's essentially it, now you plug the board in and you're ready to go.

I got all of my scintillators (used NaI(Tl), LYSO, ...) from eBay. Just search for some keywords or specific types, you'll definitely find something! Otherwise you can obviously also buy brand-new scintillators, however, these are very expensive (depends, but a factor of 10x is normal). Just be sure to look out for signs of wear and tear like scratches on the window or yellowing in NaI crystals as these can deteriorate performance significantly.

## Software

### Raspberry Pi Pico

Programming is done using the Arduino IDE. The so-called "sketch" can be found in `/arduino`.

To program the Pico you will need the following board configs:

* [Arduino-Pico](https://github.com/earlephilhower/arduino-pico)

The installation and additional documentation can also be found there. You will also need the following libraries. In addition, I wrote my own little library to calibrate the Pico's ADC using a simple linear calibration which is also used:

* [Arduino-Pico-Analog-Correction](https://github.com/Phoenix1747/Arduino-Pico-Analog-Correction) ![arduino-library-badge](https://www.ardu-badge.com/badge/PicoAnalogCorrection.svg?)
* [SimpleShell](https://github.com/cafehaine/SimpleShell) ![arduino-library-badge](https://www.ardu-badge.com/badge/SimpleShell.svg?)

They can be installed by searching the name using the IDE's library manager.

#### Serial Interface

You can also control your spectrometer using the serial interface. The following commands are available, a trailing `-` indicating an additional parameter is needed for the call.

Commands:
- ``read temp`` reads the µC temperature in °C.
- ``read vsys`` reads the board's input voltage.
- ``read usb`` true or false depending on a USB connection. Thus always true if you can read it.
- ``read cal`` reads the calibration values from Arduino-Pico-Analog-Correction.
- ``cal calibrate -`` calibrates the ADC using Arduino-Pico-Analog-Correction. The parameter being the number of measurements used to average the reading, e.g. `cal calibrate -5000`.
- ``ser int -`` Enable or disable the event serial output. Takes `enable` or `disable` as parameter, e.g. `ser int -disable`.
- ``ogc info`` prints misc information about the firmware.


### PC

To get the data from the detector the serial-over-USB port is used. You can of course use any serial monitor or dump the output to a file, etc.

I have also programmed a custom analyzer web-app called [Gamma MCA](https://spectrum.nuclearphoenix.xyz/) where you can import these serial output files as well as connecting straight to the serial port and plot the data live. Please head to the [repository](https://github.com/Open-Gamma-Project/Gamma-MCA) to find more specific info about this project.

## Example Spectra

Spectrum of a tiny (~5g) LYSO scintillator showing all three distinct gamma peaks (88.34, 201.83, 306.78 keV) with an additional ~55 keV X-ray peak:

![Lu-176 spectrum](docs/lu-176.png)

Spectrum of a standard household (ionization) smoke detector. Contains roughly 0.9 µC of Am-241. Gamma peaks at 26.34 and 59.54 keV:

![Am-176 spectrum](docs/am-241.png)

Spectrum of a small tea cup with pure Uraninite (Pitchblende) glaze. You can see isotopes of the Uranium series and also a distinct U-235 plateau.

![Uraninite Glaze](docs/glaze.png)

## Some Ideas

#### Cooling the SiPM

During operation all the electronics including the photomultiplier naturally slightly heat up. Due to the detector board being connected only by a single pin connector all of it's heat shouldn't affect the SiPM PCB much. Also due to the SiPM being connected to a rather big copper area of the PCB it's heat should not increase the temperature much over ambient (if at all that is). So air or water cooling alone won't improve performance significantly. However, you could cool the SiPM PCB with a peltier module to sub-ambient temperatures. According to the [datasheet AND9770 (Figure 27)](https://www.onsemi.com/pub/Collateral/AND9770-D.PDF) every 10°C reduction in temperature decreases the dark count rate by 50%! But be sure to correct the overvoltage in this case as it also changes with temperature.

Note that the required breakdown voltage of the SiPM increases linearly with 21.5 mV/°C, see the [C-Series SiPM Sensors Datasheet](https://www.onsemi.com/pdf/datasheet/microc-series-d.pdf). This means that you would also need to temperature correct the PSU voltage if you wanted to use it with considerably different temperatures.

#### Shielding Background Radiation

Shielding the ambient background can be done ideally using a wide enough layer of lead (bricks) all around the detector with a thin layer of lower-Z material on the inside (to avoid backscattering). The SiPM and the sample can then be put into the structure to get the best measurements possible (low background).

See Wikipedia: [Lead Castle](https://en.wikipedia.org/w/index.php?title=Lead_castle&oldid=991799816)

---

Thanks for reading.
