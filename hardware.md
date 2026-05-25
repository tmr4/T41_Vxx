# Accomodating Hardware Differences

This software supports several versions of T41 hardware.

## Ongoing Work

Most hardware specific code has been pulled out of the core code into hardware specific modules.  This work will continue to be refined.  A new module for the T41 Mini is planned.

## Background

### Project Structure

A handy Arduino feature makes maintaining the common project easy. The Arduino compiler will compile any source and header files in the sketch folder ***and*** files and subfolders in the *src* subfolder. All other subfolders in the sketch folder are ignored. That makes the following folder structure possible:

![Project folder structure](https://github.com/tmr4/T41_Vxx/blob/main/images/CommonCodeFolderStructure.png)

The T41 sketch and common code files are placed in the *T41_SDR* folder. The hardware specific files are place in the *v11*, *v12*, *vMini*, *vPS*, or *vPT* folders respectively. Then, if you want to compile for a specific hardware version, you copy the files from the desired hardware version folder into the *src* folder, select the proper Teensy in the IDE and compile.  You can't copy the hardware specific folder as the common code in the sketch folder references commonly named hardware specific files in the src folder.

I've also created display specific modules.  Select the display folder appropriate for your hardware and copy the entire folder into the src folder.  The *displayRA8875* folder contains the routines for rendering the T41 diplay on an RA8875 display.  The *displayILI9341* folder contains a few routines for simplified T41 diplay on an ILI9341 display. The *displayNone* folder contains empty versions all of the display functions needed for the T41 code to compile and run.  Unlike other versions of the code, my code doesn't rely on the display routines to regulate the T41 audio stream.  The *displayNone* folder is a good starting point for developing a new display module.  Just code the T41 display features you want to display and the core code will update the display at the appropriate time.  Of course, if you want some new feature, that can be coded from scratch.

Generally, other files shouldn't refer to these files as the folder name changes with the display.  I may relax this in the future.  Currently, the calibration reoutine don't follow this rule.  There is a lot there to separate out to common, hardware and display specific code.  Note that the calibration routines are broken currently.

When managing the project, it's best to keep the *src* folder free of hardware specific files when committing changes.  Just copy any changed hardware files back to the specific hardware version folder, delete the hardware files in the *src* folder and proceed as normal.  Note that the *ft8_lib* folder should remain in the *src* folder.

A minimum hardware version is available in the *minHardware* folder.  This allows testing of the receive function on the main board with appropriate IQ signal input.  An appropriate display or *displayNone* should be used.

In anticipation of the T41 Mini, I've extracted the remaining hardware specific routines core code.  I've also added a few hardware specific folders (vPS and vPT) for the ProtoSupplies.com [Project System](https://protosupplies.com/product/project-system-for-teensy-4-1/) and [Prototyping System](https://protosupplies.com/product/prototyping-system-for-teensy-4-1//).  I use these boards frequently in tests where I don't want to load the updated T41 software onto an actual radio.

## Creating a new hardware version

To come.
