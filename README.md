# Open Gamma Detector

__This project is on [Hackaday.io](https://hackaday.io/project/185211-all-in-one-gamma-ray-spectrometer), where I also post project logs, progress updates and other announcements.__

Open hardware for a simple, yet powerful scintillation counter and multichannel analyzer (MCA) all-in-one device using a popular NaI(Tl) scintillation crystal. Suitable for (DIY) gamma spectroscopy while being significantly cheaper than any off-the-shelf platform. Uses a standard serial-over-USB connection so that it can be integrated into as many other projects as possible, for example data logging with a Raspberry Pi, weather stations, Arduino projects, etc.

Hardware design has been done with [EasyEDA](https://easyeda.com/) and all the needed files for you to import the project as well as the schematic can be found in the `hardware` folder. There is also a Gerber file available for you to go directly to the PCB manufacturing step.

The software aims to be as simple as possible to understand and maintain; to achieve this I decided to use an off-the-shelf microcontroller - the [Raspberry Pi Pico](https://www.raspberrypi.com/products/raspberry-pi-pico/). This board can be programmed with the Arduino IDE over micro-USB and is powerful (dual core, good ADC, plenty of memory, ...) enough for the purpose and also exceptionally cheap.

For quick access and purchase of all the parts (PCB and BOM) you can use [Kitspace](https://kitspace.org/boards/github.com/open-gamma-project/open-gamma-detector/). In addition, you'll also need to buy the SiPM (MICROFC-60035-SMT-TR1) at a distributor of your choice.

_The fully assembled main detector board with the Pico on it will also be available for purchase [soon](https://hackaday.io/project/185211-all-in-one-gamma-ray-spectrometer/log/208359-assembled-detector-boards)._

## Key Features

Here are some of the most important key facts:

* Compact design: Total size 120 x 50 mm. 70 x 50 mm area for electronics and additional 50 x 50 mm to mount the scintillator.
* All-in-one detector: No external parts (e.g. sound card) required to record gamma spectra.
* Micro-USB serial connection and power.
* Easily programmable using the standard Arduino IDE.
* Low-voltage device: No HV needed for a photomultiplier tube.
* SiPM voltage range from 28 V to 33 V.
* Low power consumption: ~25 mA @ 5 V.
* Adjustable preamp gain for the SiPM pulses (affects energy range & resolution).
* Default Mode: Capable of up to around 40,000 cps while also measuring energy.
* Geiger Mode: Capable of up to around 100,000 cps without energy measurement.
* 4096 ADC channels for the energy range of about 30 keV to 1300 keV.
* Additional broken-out power pins and I2C header for displays, etc.

## Working Principle

<p align="center">
  <img alt="Working Principle Flow Chart" title="Working Principle Flow Chart" src="docs/flow.drawio.png">
</p>

## Hardware

This project utilizes a silicon photomultiplier (short SiPM) which is way smaller and more robust than a traditional photomultiplier tube and does not need a high-voltage supply (traditionally ~1000 V). Here are some very helpful in-depth datasheets about the particular MicroFC SiPM used here:

* [C-Series SiPM Sensors datasheet](https://www.onsemi.com/pdf/datasheet/microc-series-d.pdf)
* [Linearity of the Silicon Photomultiplier](https://www.onsemi.com/pub/Collateral/AND9776-D.PDF)
* [Introduction to the SiliconPhotomultiplier (SiPM)](https://www.onsemi.com/pub/Collateral/AND9770-D.PDF)
* [Biasing and Readout of ON Semiconductor SiPM Sensors](https://www.onsemi.com/pub/Collateral/AND9782-D.PDF)

The hardware consists of the main detector (`hardware` folder) which includes amplification, pulse detection and energy measurement. If you already have a SiPM/crystal assembly compatible with voltages around 30 V, you may use it with the detector board and soldering wires directly to the correct pads. Otherwise, you can use my [SiPM carrier board](https://github.com/Open-Gamma-Project/SiPM-Carrier-Board), which holds the SiPM and all the optimal decoupling.

The heart of the detector board is the Raspberry Pi Pico which uses its ADC to measure the pulse amplitude (i.e. the energy) immediately after an event occurs starting with an interrupt. I can really recommend you reading the datasheet or maybe also having a look at a deeper analysis of the Pico ADC, if you're interested:

* [Raspberry Pi Pico](https://datasheets.raspberrypi.com/pico/pico-datasheet.pdf)
* [Characterizing the Raspberry Pi Pico ADC](https://pico-adc.markomo.me/)

Here are some front and back side renders of the detector PCB. Size is about 12 x 5 cm. If you don't need the additional space to mechanically mount the SiPM/scintillator assembly to the rest of the detector board, you can just cut it off at the white line and you're left with a smaller detector.

<p align="center">
  <img alt="Front Side PCB" title="Front Side PCB" src="docs/pcb_front.png" style="width:49%">
  <img alt="Back Side PCB" title="Back Side PCB" src="docs/pcb_back.png" style="width:49%">
</p>

On the back side of the PCB there is place for two optional components:
* a voltage reference for the ADC (LM4040AIM3-3.0+T recommended) to get rid of most power supply related noise and inaccuracy
* and a 0 Ω link to connect the analog ground to the rest of the ground plane.

These can be retrofitted easily and are quite affordable. Both are optional and should only be used if you know what you are doing.

There are also broken-out pins for the power supply and I2C connections. These can be used to modify the device, e.g. by adding a display or using a battery charger.

### Scintillator Assembly

The finished SiPM carrier board is there to allow for easier packaging with the scintillator as well as to be reusable for different detectors as that's by far the single most expensive part and you'll want to use it as long as possible. You should apply some optical coupling compound between the SiPM and the crystal window to reduce reflections as good as possible (this way the best photon detection efficiency is achieved). There are also special materials for this use case but you can also use standard silicone grease - works great for me. After you applied some, you press both parts together and wrap everything with light-tight tape, again, I'm just using some black electrical tape here. That's essentially it, now you can solder some wires to the pads on the board to connect them together and secure it in place in the free space on the board. There are holes on each side of the PCB for some cable ties.

I got all of my scintillators (used NaI(Tl), LYSO, ...) from eBay. Just search for some keywords or specific types, you'll probably find something! Otherwise you can obviously also buy brand-new scintillators, however, these are much more expensive (depends, but a factor of 10x is normal). Just be sure to look out for signs of wear and tear like scratches on the window or yellowing (!) in NaI crystals as these can deteriorate performance significantly.

## Software

### Raspberry Pi Pico

Programming is done using the Arduino IDE. The so-called "sketch" (i.e. the programmed software) can be found in `/arduino`.

To program the Pico you will need the following board configs:

* [Arduino-Pico](https://github.com/earlephilhower/arduino-pico)

The installation and additional documentation can be found in the respective GitHub repo, it's not complicated at all and you only need to do it once. You will also need the following additional libraries:

* [Arduino-Pico-Analog-Correction](https://github.com/Phoenix1747/Arduino-Pico-Analog-Correction) ![arduino-library-badge](https://www.ardu-badge.com/badge/PicoAnalogCorrection.svg?)
* [SimpleShell](https://github.com/cafehaine/SimpleShell) ![arduino-library-badge](https://www.ardu-badge.com/badge/SimpleShell.svg?)

They can be installed by searching their names using the IDE's built-in library manager.

#### Serial Interface

You can control your spectrometer using the serial interface. The following commands are available, a trailing `-` indicating an additional parameter is needed at the end of the command.

Commands:
- ``read temp`` reads the microcontroller temperature in °C.
- ``read vsys`` reads the board's input voltage.
- ``read usb`` true or false depending on a USB connection. Thus always true if you are using the serial-over-USB connection.
- ``read info`` prints miscellaneous information about the firmware.
- ``read cal`` reads the calibration values from Arduino-Pico-Analog-Correction.
- ``read spectrum`` reads the histogram data of all energy measurements since start-up.
- ``set mode -`` use either `geiger` or `energy` mode to disable or enable energy measurements respectively. Geiger mode only measures counts per second, but has a 3x higher saturation limit.
- ``cal calibrate -`` calibrates the ADC using Arduino-Pico-Analog-Correction. The parameter being the number of measurements used to average the reading, e.g. `cal calibrate -5000` takes 5000 measurements.
- ``ser int -`` Change or disable the event serial output. Takes `events`, `spectrum` or `disable` as parameter, e.g. `ser int -disable` to disable event outputs. `spectrum` will regularly print the full ready-to-use gamma spectrum. `events` will print all the registered new events in chronological order.

### PC

To get the data from the detector the serial-over-USB port is used by default. The quickest and easiest way to do this is by using my own web application called [Gamma MCA](https://spectrum.nuclearphoenix.xyz/) where you can connect straight to the serial port and plot the data live as well as import and export finished spectrum files. You don't even need to install it, it can work out of any Chrome-based browser! Please head to the [repository](https://github.com/Open-Gamma-Project/Gamma-MCA) to find more specific info about this project.

You can of course use any other serial monitor or gamma-spectroscopy software that's compatible with serial connections. There isn't much, though, that's why I made one myself.

## Example Spectra

1 hour long background spectrum with no samples:

![Background spectrum](docs/bg.png)

Spectrum of a tiny (~5 g) LYSO scintillator inside some lead shielding showing all three distinct gamma peaks (88.34, 201.83, 306.78 keV) with an additional ~55 keV X-ray peak (4h measurement):

![Lu-176 spectrum](docs/lu-176.png)

The same LYSO sample with the same lead shielding, but using a newer "9%" NaI(Tl) scintillator and only a 2h measurement. Energy resolution @ 307 keV is now about 15% now instead of the ~18% above:

![Lu-176 spectrum](docs/lu-176-new.png)

Spectrum of a standard household ionization smoke detector. Contains roughly 0.9 µC of Am-241. Gamma peaks at 26.34 and 59.54 keV:

![Am-176 spectrum](docs/am-241.png)

Spectrum of a small tea cup with pure Uraninite (Pitchblende) contents in its glaze. You can see all kinds of isotopes of the Uranium series and also a distinct U-235 plateau:

![Uraninite Glaze](docs/glaze.png)

## Known Limitations

1. The Raspberry Pi Pico's ADC has some pretty [severe DNL issues](https://pico-adc.markomo.me/INL-DNL/#dnl) that result in four channels being much more sensitive (wider input range) than the rest. For now the simplest solution was to discard all four of them, by printing a `0` when any of them comes up in the measurement (to not affect the cps readings). This is by no means perfect or ideal, but it works for now until this gets fixed in a later hardware revision of the RP2040.

2. It's very important to get the SiPM/scintillator assembly light-tight. Otherwise you'll either run into problems with lower energies where noise dominates or outright not measure anything at all, because the sensor is saturating.

3. At the moment the detector board only works with a fixed preamp gain (if you didn't change it) and fixed SiPM voltage. In [the near future](https://hackaday.io/project/185211-all-in-one-gamma-ray-spectrometer/log/208062-new-detector-revision-coming-soon) this will be addressed to allow many different kinds of SiPMs and scintillators to be used.

## Some Ideas

#### Cooling the SiPM

During operation all the electronics including the photomultiplier naturally slightly heat up. Due to the detector board being connected only by a single pin connector all of it's heat shouldn't affect the SiPM PCB much if at all. Also due to the SiPM being connected to a rather big copper area of the PCB it's heat should not increase the temperature significantly over ambient. So air or water cooling alone won't improve performance considerably. However, you could cool the SiPM PCB with a peltier module to sub-ambient temperatures. According to the [datasheet AND9770 (Figure 27)](https://www.onsemi.com/pub/Collateral/AND9770-D.PDF) every 10°C reduction in temperature decreases the dark count rate by 50%! But be sure to correct the SiPM voltage (overvoltage) in this case as it also changes with temperature.

Note that the required breakdown voltage of the SiPM increases linearly with 21.5 mV/°C, see the [C-Series SiPM Sensors Datasheet](https://www.onsemi.com/pdf/datasheet/microc-series-d.pdf). This means that you would also need to temperature correct the PSU voltage if you wanted to use it with considerably different temperatures.

#### Shielding Background Radiation

Shielding the ambient background can be done ideally using a wide enough layer of lead (bricks) all around the detector with a thin layer of lower-Z material on the inside (to avoid backscattering). The SiPM and the sample can then be put into the structure to get the best measurements possible (low background).

See Wikipedia: [Lead Castle](https://en.wikipedia.org/w/index.php?title=Lead_castle&oldid=991799816)

---

Thanks for reading.
