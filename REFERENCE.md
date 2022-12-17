# Troubleshooting and reference

## Settings Reference

### SiPM Voltage

todo

### Preamp Gain

todo

### Discriminator Threshold

todo

### Input Resistor R4

The resistor R4 on the front side is optional. It will raise the input voltage of the preamp and therefore also the output so that even lower signals are above the inherent swing of the op amp at a given gain.

This way you _might_ be able to read lower energies for a fixed gain up to the noise level. If you cannot get any signal whatsoever, you can also try and solder this resistor, which might fix your problem.

Due to the preamp as well as SiPM gain being variable now and this voltage divider also introducing some noise into the signal I opted to leave this part out by default.

---

## Troubleshooting

### My device freezes and/or is not recognized.

Re-flash the firmware, reset all the settings and reboot the device. If the problem persists, please feel free to open an issue on GitHub.

### todo

todo

---

## FAQ

### How to update the firmware?

Press and hold the "BOOTSEL" button on the Raspberry Pi Pico while plugging it into your USB port. It will then show up in your file explorer and you can once again drag-and-drop the firmware file. This also works if your device is completely frozen or crashed.

### Can I use a plastic scintillator?

No, these cannot be used for gamma spectroscopy except in some edge cases. Generally, they have much worse efficiency and performance than your typical inorganic scintillator, like NaI(Tl) or LYSO.

### Can I use the non-voltage reference firmware and vice versa?

Yes, this doesn't affect the performance at all. The only readings that uses the reference voltage value is the temperature sensor and the supply voltage reading. These will be unusable if the setting is wrong, but are not used in any way for the spectroscopy functions.

### How do I change the x-axis from the ADC channels to energy (keV)?

You have to get a material with at least two known gamma peaks and calibrate the detector using these two peaks. Otherwise it is not possible.

### todo

todo
