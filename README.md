#### The most recent features, [see recent work](https://github.com/tmr4/T41_Vxx/blob/main/RecentWork.md), in this branch work with hardware versions *v11*, and my T41 and remote test beds, *vPS* and *vAP*. They won't be incorporated into hardware version *v12* until I get back to building it.

# T41_Vxx

This is a combination of my T41 v11 and v12 projects, creating a consistent code base for my T41 projects.  The combinations should make it easier to maintain consistency and allow me to more easily add new features and hardware versions.

![T41 new display layout](https://github.com/tmr4/T41_Vxx/blob/main/images/T41_newDisplayLayout.jpg)

## Features

Large portions of original SDTVer049.2k code have been reworked to be more efficient and/or use less memory. This has allowed adding a lot of new features without impacting performance.

T41_Vxx supports the following added features. For more detail on some of the older features see [features](https://github.com/tmr4/T41_Vxx/blob/main/features.md). Newer features may be discussed in my recent work entries or on [Reddit](https://www.reddit.com/r/T41_EP/). Some features require additional hardware:

* mouse and keyboard support
* keyboard/memory CW keyer
* CAT control
* FT8: WSJT-X w/ audio over USB; internal FT8 (RX/TX), requires PSRAM
* Control T41 with plug and play USB/Ethernet connection to remote head
* NFM demodulation
* beacon monitor, remote display over bluetooth, USB host connection to another T41
* more efficient display layout; live noise floor, active info box
* support for test platforms and alternate display framework
* LDG antenna tuner Y-ACC-1 cable support

## Other Views

### Standalone Internal FT8

![T41 Internal FT8 contact](https://github.com/tmr4/T41_Vxx/blob/main/images/T41_ft8.jpg)


### Remote Unit

![Desktop Companion](https://github.com/tmr4/T41_Vxx/blob/main/images/T41_AP_Bedside.png)

### T41 Test Bed

![T41 new display layout](https://github.com/tmr4/T41_Vxx/blob/main/images/T41_FrontPanelBox.jpg)

### Beacon Monitor

![Beacon Monitor with random SNR](https://github.com/tmr4/T41_Vxx/blob/main/images/bm_random_snr.jpg)

## Ongoing and Recent Work Blog

See [Ongoing and Recent Work](https://github.com/tmr4/T41_Vxx/blob/main/RecentWork.md) more more detail of some recent work.

## Use

This software supports several versions of T41 hardware. To customize the software for specific hardware version, copy the files from the desired hardware subfolder (in the *T41_SDR/hardware* folder) and paste them into the *T41_SDR/src* folder.

Support for the T41 display is handled differently. To add support for a specific display, copy the folder for the desired display (in the *T41_SDR/hardware/diplays* folder) and paste the entire folder into the *T41_SDR/src* folder.

When managing the project, it's best to keep the *src* folder free of hardware specific files when committing changes.  Just copy any changed hardware files back to their specific hardware version folders, delete the hardware files in the *src* folder and proceed as normal.  Note that the *ft8_lib* folder should remain in the *src* folder. An alternative is to add the *src* folder to your *.gitignore* file. This has the downside of any changes you make to the files in the source folder won't be highlighted during your coding.

For more detail see [Accomodating Hardware Differences](https://github.com/tmr4/T41_Vxx/blob/main/hardware.md).

### Caution

I've disabled saving operating parameters in EEPROM to facilitate changing versions. Thus you can try out this software without overwriting the EEPROM data from the version you normal uses. I get around this limitation by hardcoding my preferences. The problem is these aren't your preferences.

This is a work in progress.  Some functions may be broken and will likely remain so until they are of interest to me.  This is especially true for the v12 radio. I'm still building that so I've yet to add a lot of v12 functionality.  I don't plan on upgrading my AI6YM v12 kit to the latest v12 hardware revision but I've found many hardware features easy to add.

Use at your own risk.

## Pin Usage

See [pinout](https://github.com/tmr4/T41_Vxx/blob/main/Pinout.md).

## Other T41 Related Apps

See [Other apps](https://github.com/tmr4/T41_Vxx/blob/main/OtherT41Apps.md)

  * PC beacon monitor
  * PC control
  * PC debug window
  * Set T41 clock
  * Interconnect T41 radios
  * Remote display
