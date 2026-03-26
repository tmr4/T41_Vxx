# T41_Vxx

This is a combination of my T41 v11 and v12 projects.  This should make it easier to maintain consistency between the two versions.

The driver for this project was to leverage all of the work I put into adding features to my v11 radio such as:

* new input capability (mouse and keyboard)
* new modes (NFM demodulation and some data modes)
* new features (beacon monitor, CW message keyer, CAT control, remote display, USB host connection to another T41)

In anticipation of the T41 Mini, I've extracted the hardware specific routines from the Button and Encoder modules.  I've also added a hardware specific folder (vPS) for the [ProtoSupplies Project System](https://protosupplies.com/product/project-system-for-teensy-4-1/).  I use that board frequently in tests where I don't want to load the updated T41 software onto an actual radio.

This is a work in progress.  Some functions may be broken and will likely remain so until they are of interest to me.  This is especially true for the v12 radio. I'm still building that so I've yet to add a lot of functionality.  Use at your own risk.

## Recent Work

* WSJT-X working over T41 USB connection
  * Modified the FT8 Data mode to pass audio back and forth with a PC over USB at a 44.1kHz sample rate
  * Modified the wsjt module CAT controls for transmit
  * T41 switches to FT8 Data mode upon receipt of *ID;* command
  * Calibration of FT8 Data mode still in progress
* Standalone FT8 based on [ft8_lib](https://github.com/kgoba/ft8_lib)
  * a modified version of the library for the T41 is in the *ft8_lib* folder within *src* folder and available to all hardware versions
  * This mode is available with a special data mode, internal FT8 mode
  * The mode can also play wav files.
  * FT8 UI is mouse driven at present

![T41 Internal FT8 contact](https://github.com/tmr4/T41_Vxx/blob/dev/v0.01/images/T41_ft8.jpg)

The standalone FT8 interface is limited due to the display size.  Currently three message lists are available, all scrollable with a mouse wheel.  To the left is a list of all recent FT8 messages.  In the middle is a list of all recent CQ messages.  To the right is a list of all messages around the selected FT8 receive frequency as entered at the bottom of the infomation box.  The bottom lines of the display show the current/most recent QSO.  The next FT8 message to be transmitted is shown in green in the last line.  Yellow messages are completed.  The line above the message lists shows the details of the selected FT8 message, which can be changed by clicking on a message in any of the lists.

* New display layout
  * captures some unused space, expanding the area for FT8 messages and more info box items

![T41 new display layout](https://github.com/tmr4/T41_Vxx/blob/dev/v0.01/images/T41_newDisplayLayout.jpg)

## Use

A handy Arduino feature makes maintaining the common project easy. The Arduino compiler will compile any source and header files in the sketch folder ***and*** the *src* subfolder. All other subfolders in the sketch folder are ignored. That makes the following folder structure possible:

![Project folder structure](https://github.com/tmr4/T41_Vxx/blob/dev/v0.01/images/CommonCodeFolderStructure.png)

The T41 sketch and common code files are placed in the *T41_SDR* folder. The hardware specific files are place in the *v11*, *v12*, *vMini*, or *vPS* folders respectively. Then, if you want to compile for a specific hardware version, you copy the files from the desired hardware version folder into the *src* folder, select the proper Teensy in the IDE and compile.

When managing the project, it's best to keep the *src* folder free of hardware specific files when committing changes.  Just copy any changed hardware files back to the specific hardware version folder, delete the hardware files in the *src* folder and proceed as normal.  Note that the *ft8_lib* folder should remain in the *src* folder.
