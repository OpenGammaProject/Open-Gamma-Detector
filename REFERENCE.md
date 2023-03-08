# Troubleshooting and FAQ

## Troubleshooting

### My device freezes or crashes periodically

Make sure that the threshold setting is not set too low! If that's the case you should see the ACTivity LED blinking heavily or even staying on solid. In this case, noise is probably overwhelming the detector and forcing it to reset.

If this is not the case for you, reflash the firmware, reset all the settings and reboot the device. If the problem persists, please feel free to open an issue on GitHub.

### My device is not recognized by my computer

Make sure you are using a micro-USB cable that can be used for data transmission. There are micro-USB cables for powering/charging devices only and they can be easily mistaken for a data cable.

If this isn't the case for you and it doesn't get recognized at all (even while holding the `BOOTSEL` button), there might be a manufacturing defect or you might have made a mistakes when soldering the parts. Feel free to get in touch if that's the case.

### Settings on the device are not saved

If you flashed it via the Arduino IDE be sure to select `Flash Size: "2MB (Sketch: 1984KB, FS: 64KB)"`. Without these 64 KB of flash assigned to the file system, the Pico is unable to create and save a settings file.

---

## FAQ

### How to update the firmware?

Press and hold the "BOOTSEL" button on the Raspberry Pi Pico while plugging it into your USB port. It will then show up in your file explorer and you can once again drag-and-drop the firmware file. This also works if your device is completely frozen or crashed.

### Can I use a plastic scintillator?

No, these cannot be used for gamma spectroscopy except in some edge cases. Generally, they have much worse efficiency than your typical inorganic scintillator, like NaI(Tl) or CsI(Tl), and extremely poor energy resolution.

### How do I calibrate the device?

You have to get a material with at least two known gamma peaks and calibrate the detector using these two peaks. In the Gamma MCA calibration tab you select the two peak ADC channels and assign their gamma-ray energy.

Ideally you want to use three peaks distributed evenly over your whole energy range to use the best calibration.
