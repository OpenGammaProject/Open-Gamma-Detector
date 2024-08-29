# Troubleshooting and FAQ

## Troubleshooting

### My device freezes or crashes periodically

Make sure that the threshold setting is not set too low! If that's the case you should see the ACTivity LED blinking heavily or even staying on solid. The buzzer might also give you a great indication of the correct count rate setting. In this case, noise is probably overwhelming the detector and forcing it to reset.

If this is not the case for you, reflash the firmware, reset all the settings and reboot the device. If the problem persists, please feel free to open an issue on GitHub.

### My device is not recognized by my computer

Make sure you are using a micro-USB cable that can be used for data transmission! There are micro-USB cables for powering/charging devices only and they can be easily mistaken for a data cable.

If this isn't the case for you and it doesn't get recognized at all (even while holding the `BOOTSEL` button), there might be a manufacturing defect or you might have made a mistakes when soldering the parts. Feel free to get in touch if that's the case.

### Settings on the device are not saved

If you flashed it via the Arduino IDE be sure to select `Flash Size: "4MB (Sketch: 1MB, FS: 3MB)"`. Without these 64 KB of flash assigned to the file system, the Pico is unable to create and save a settings file.

### ~~I am seeing a sharp peak immediately around ADC channel 511~~

~~That is right, this is due to the DNL issues with the RP2040 ADC as described in the [Known Limitations](README.md#known-limitations) section of the readme.~~

~~This effect would have been much worse without some simple corrections in the firmware. Since there is currently no hardware fix, this is what we have to live with unfortunately. If you know about it, you can just ignore it since all the *real* peaks of the scintillator are much wider than that.~~ Fixed by the Pico 2.

### ~~There is always a peak at ADC channel 0~~

~~That is intentional behavior of the device. For the same reason as above, four ADC channels are ignored for the energy measurement.~~

~~Since this would potentially highly influence the count rate, giving lower values than there actually are, these counts are added back to the spectrum to ADC channel 0.~~

~~This way, all the counts are registered, but since there is actually never a signal near channel 0, you can clearly distinguish between the "right" spectrum and the rest.~~

~~If you want to disable this behavior, you can do so by using the  `set correction` command over the serial interface. This will still omit the ADC channel readings, but won't create another peak at 0. In geiger mode, there will always only be a channel `0` since this is how the current cps is calculated.~~  Fixed by the Pico 2.

### There is always a peak near ADC channel 4095

The ADC reference voltage is set to 3 Volts. Anything larger than that gets clipped, because the ADC cannot read higher voltages, meaning all pulses larger than 3V accumulate at the maximum ADC channel -- in this case 4095. That is completely normal behavior.

---

## FAQ

### How to update the firmware?

Press and hold the "BOOTSEL" button on the Raspberry Pi Pico while plugging it into your USB port. It will then show up in your file explorer and you can once again drag-and-drop the firmware file. This also works if your device is completely frozen or crashed.

### Can I use a plastic scintillator?

No, these cannot be used for gamma spectroscopy except in some edge cases. Generally, they have much worse efficiency than your typical inorganic scintillator, like NaI(Tl) or CsI(Tl), and extremely poor energy resolution.

If you want to *just* count pulses (i.e. **not** measure pulse energy), plastic scintillators can be a better fit, because of their really low cost. If you want to count pulses only, though, it's probably better to use the simpler and much cheaper [Mini SiD](https://github.com/OpenGammaProject/Mini-SiD) board instead of the Open Gamma Detector. With plastic scintillators you can also detect high-energy particles such as beta radiation or muons, which NaI(Tl) generally cannot.

### How do I change the gain?

Since hardware revision 3 the gain is fixed on all boards and cannot be changed without any hardware modifications. This makes it **much** easier to provide a device that has a very high chance of working out of the box without much fiddling around with pot settings or additional components.

A downside of this is that different scintillator sizes, while working most of the time now, provide slightly different energy ranges and therefore bin per energy resolutions. Normally, this shouldn't be a problem, though.

If you want to change the gain for whatever reason, you can do so by soldering the optional trim pot on the board (R20) with a [compatible pot (e.g. 5k)](https://www.lcsc.com/product-detail/Variable-Resistors-Potentiometers_BOURNS-3314J-1-502E_C58391.html). Be sure to also remove the existing 0605 resistors R7 and R11 in that case. You can then change the gain from roughly 1 to 6 on the fly.

### How do I calibrate the device?

You have to get a material with at least two known gamma peaks and calibrate the detector using these two peaks. In the Gamma MCA calibration tab you select the two peak ADC channels and assign their gamma-ray energy.

Ideally you want to use three peaks distributed evenly over your whole energy range to use the best calibration.

### Why don't you use a TIA or CSP or change X or Y?

This device is made to be as simple and cheap as possible, while still yielding as great results as possible. In effect it is a compromise to get the best price-performance ratio.

Of course, you could use transimpedance or charge-sensitive preamplifiers, however this would significantly increase complexity as you'd need additional parts for the correct power supplies and so on. Using more expensive amplifiers would surely also be an option.

If you wanted to go the maximum-performance route, you'd also need to use a different microcontroller with a much better ADC, and maybe even consider using 4-layer boards with additional ground and power planes. Essentially, you'd have to re-design the whole board at this point.

Instead of something like 8-10% energy resolution @ 662 keV (highly dependent on your SiPM/scintillator assembly), you'd then maybe get as much as 6% if you're lucky, which is the limit for most NaI or CsI scintillators anyways. That would come at a much higher cost, though. In my opinion, you get to a point of diminishing returns quickly, especially when designing an entry-level device such as this.

### Notes on different microcontrollers

The Pico 2 is readily available virtually everywhere and it's really cheap. This makes it ideal for use in this kind of "simple" and cheap project. There are a couple of reasons I like to use it. You can read more about it in the [discussions thread about switching to the ESP32](https://github.com/OpenGammaProject/Open-Gamma-Detector/discussions/43).
