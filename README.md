# T41_Vxx

This is a combination of my T41 v11 and v12 projects.  This should make it easier to maintain consistency between the two versions.

The driver for this project was to leverage all of the work I put into adding features to my v11 radio such as:

* new input capability (mouse and keyboard)
* new modes (NFM demodulation and some data modes)
* new features (beacon monitor, CW message keyer, CAT control, remote display, USB host connection to another T41)

This is a work in progress.  Some functions may be broken and will likely remain so until they are of interest to me.  This is especially true for the v12 radio. I'm still building that so I've yet to add a lot of functionality.  Use at your own risk.

## Recent Work

* WSJT-X working over T41 USB connection
  * Modified the FT8 Data mode to pass audio back and forth with a PC over USB at a 44.1kHz sample rate
  * Modified the wsjt module CAT controls for transmit
  * T41 switches to FT8 Data mode upon receipt of *ID;* command
  * Calibration of FT8 Data mode still in progress

## Use

A handy Arduino feature makes maintaining the common project easy. The Arduino compiler will compile any source and header files in the sketch folder ***and*** the *src* subfolder. All other subfolders in the sketch folder are ignored. That makes the following folder structure possible:

![Project folder structure](https://github.com/tmr4/T41_Vxx/blob/dev/v0.01/images/CommonCodeFolderStructure.png)

The T41 sketch and common code files are placed in the *T41_SDR* folder. The hardware specific files are place in the *v11* and *v12* folders respectively. Then, if you want to compile for a v11 or v12 radio, you put the hardware specific files for the desired version in the *src* folder, select the proper Teensy in the IDE and compile.

Alternatively, you can maintain version specific branches where the *src* folder is already properly set up.  I'm still experimenting with this alternative.  Git source control tracks file changes overtime.  However, the contents of the *src* folder depends on the version. This doesn't work well with time-based source tracking.  When creating a version specific development branch for updating a version branch, I had to resolve merge conflicts with at least one version.  A better approach might be to keep the *src* folder empty in the development branch and then manually add the hardware specific files to the *src* folder for each version branch.  With this alternative, the *src* folder doesn't even exist in the GitHub repository, but you can add it to your local copy for testing.  Then just copy the hardware specific files for the version you're interested in testing.  Delete them before you're ready to commit and all should be good.

## Branches

* dev/v0.01 - development branch
* v11 - select for v11 radios
* v12 - select for v12 radios
