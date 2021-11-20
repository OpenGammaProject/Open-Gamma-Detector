# Open Scintillation Counter

## !Note: This repo is still very much work in progress. I am currently working on finishing this project.

Open hardware for a simple, yet powerful, all-in-one scintillation counter design using a popular NaI(Tl)
scintillation crystal. Can be used for gamma spectroscopy while being much cheaper than any off-the-shelf platform.

Hardware design has been done mainly in EasyEDA and all the needed files for you to import the project as well as
a schematic can be found in the `electronics` folder. There is also a gerber file available for direct PCB manufacturing.

Software aims to be as simple as possible to understand and maintain; to achieve this I decided to use an off-the-shelf
microntroller - the quite new Raspberry Pi Pico. This board can be programmed over micro USB and is powerful (i.e. fast)
enough for the purpose and also pretty cheap.

## Working principle

<p align="center">
  <img src="docs/flow.drawio.png">
</p>
  
## Hardware

**To do: Add PCB images.**

**Note: The schematic is outdated, the most recent version will be here shortly.**

This project utilizes a silicon photomultiplier (short SiPM) which is way smaller and more robust than a traditional photomultiplier
tube and does not need a high-voltage supply. I'll link some in-depth datasheets about this particular SiPM:

* [C-Series SiPM Sensors datasheet](https://www.onsemi.com/pdf/datasheet/microc-series-d.pdf)
* [Linearity of the Silicon Photomultiplier](https://www.onsemi.com/pub/Collateral/AND9776-D.PDF)
* [Introduction to the SiliconPhotomultiplier (SiPM)](https://www.onsemi.com/pub/Collateral/AND9770-D.PDF)
* [Biasing and Readout of ON Semiconductor SiPM Sensors](https://www.onsemi.com/pub/Collateral/AND9782-D.PDF)

This project consists of two PCBs - one carrier board for the SiPM (`electronics/sipm` folder) that will be mounted on top of
your scintillation crystal. It will then be connected to the other board: the main detector (`electronics/detector` folder)
which includes amplification, pulse detection and energy measurement.

Attention when mounting the SiPM to its PCB: One of the four legs in each corner is slightly bigger than the rest,
this one must be lined up with the dot on the PCB!

After you soldered all the components to the SiPM PCB you can mount it to your crystal.
For that I am using a small (18x30mm) standard NaI(Tl) crystal. These can be bought cheaply second-hand from ebay or any comparable
platform. Most of the sellers I found are from Russia and the Ukraine, but that depends on where you're located.

You'll also want to optically couple the crystal to the SiPM in order to minimize reflection, therefore
increasing transmission and photon detection rates. I'm using standard silicone grease because, again, it's cheap and
readily available. Only apply a thin layer, then press the two parts together and wrap them with a couple of layers of
electrical tape so that no light can get in and overexpose the highly sensitive SiPM.

## Software

I'm currently working on my own version of an Web-MCA designed to allow uploading custom gamma spectrum data and also use the
detector output directly to do some live-plotting. More info will follow.

## Example measurements

Data will follow when the new detector is ready.

|Sample Material|Average counts per seconds (cps)|
|:------:|:-----------------------------------------:|
|Background radiation, no sample|todo|
|Am-241 (**still inside**) a regular smoke detector (0.9 micro curies)|todo|
|5.2g LYSO scintillator (Lu-176!) |todo|
|Uranium glaze on an electric plug (~1930/40s)|todo|
|Wristwatch with Radium dials|todo|
|Pitchblende (amount unknown)|todo|

## Documentation

A link to the comprehensive documentation will follow. Please be patient, this is **a lot** of work :)

It will also include the following...

### Limitations and Ideas

#### Cooling the SiPM

During operation all the electronics including the photomultiplier naturally slightly heat up. Due to the analysis hardware being connected only by a couple of wires in some (if low) distance all of it's heat shouldn't affect the SiPM PCB really. Also due to the SiPM being connected to a rather big copper area of the PCB it's heat too should not increase the temperature much over ambient (if at all that is). So air or water cooling alone won't improve performance significantly. However, one could cool the SiPM PCB with a peltier module to sub-ambient temperatures. SensL already provides peltier-cooled SiPMs with their [MiniSM](https://www.sensl.com/downloads/ds/DS-MiniSM.pdf) series of sensor modules. According to their [datasheet AND9770 (Figure 27)](https://www.onsemi.com/pub/Collateral/AND9770-D.PDF) every 10°C reduction in temperature decreases the dark count rate by 50%!

#### Shielding Background Radiation

Shielding the ambient background can be done ideally using a wide enough layer of lead (bricks) all around the detector. The SiPM and the sample can then be put into the structure to get the best measurements possible. But be sure to get "clean" lead with a low enough percentage of isotopes if you can as they too can be radioactive and introduce even more radiation to the system.

See Wikipedia: [Lead Castle](https://en.wikipedia.org/w/index.php?title=Lead_castle&oldid=991799816)
