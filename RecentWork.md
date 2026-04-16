# Ongoing and Recent Work

## Ongoing Work

### Refined hardware and new display drivers

* I've extracted most of the T41 hardware specific code into separate version specific hardware *libraries*.  The front panel and display are two areas I haven't previously attempted, for different reasons.  My original thought was to make the front panel code applicable to all versions with a group of defines activating the correct code depending on the selected front panel. This created confusing code and while seemingly practical, was never used in practice as my radios all have a version specific front panel.  It also made it more difficult to develop code for the various development boards I've been working with.
* I've completed the basic RA8875 display module and tested against the v11 and Project System hardware.  The Project System was the easiest hardware version to develop for as I haven't defined calibration code for it.  I also createda no display version that removes the body of required display functions.
* More work is needed to accomodate the calibration routines.  These are highly display specific as well as hardware specific to some extent.

### A new display driver for ILI9341

* As a proof of concept, I developed a basic display module a Teensy 4.1 Prototyping System from ProtoSupplies.  That system uses a 3.2" 320x240 ILI9341 display.
* Teensyduino supports this display, so modifying the RA8875 display module code for this display was straighforward.  Some display function used in the RA8875 module aren't supported by the ILI9341, font size functions for example.  The code needed reworked for those.
* This mock up for the ILI9341 display highlighted that much of the display code is customized for the RA8875, not just based on it's resolution, but the particular placement of elements on the display.  Many of those need reworked when moving to a new display.

![displayILI9341](https://github.com/tmr4/T41_Vxx/blob/dev/v0.01/images/T41_altDisplay_PrototypingSystem.jpg)

  * Adding a small waterfall took more work as the display doesn't have a move function.  The entire waterfall needs to be written to the display line by line each processing loop.  A circular buffer works well for this.
  * I have 2.4" and 2.8" versions of this display as well.  I thought the smaller displays would draw less current, but that's not the case.  The smallest display drew the most current and while it's a bit of an apple/oranges comparison, it drew almost as much as my v11 T41 main board with 5" RA8875 display.  Even the more efficient 3.2" version was less than 50 mamps less than that combination.  These displays aren't going to be much of a battery saver on a T41 Mini.
  * The spectrum update on the ILI9341 is quite snappy.  As with all mockups on development boards, the trickiest thing is defining the Teensy pin assignments so they don't conflict with ones being used by the development board.  The switch matrix busy pin threw me for a while with the Prototyping System.  This has taught me to have all of the Teensy pin assignment located in one place and separated by input/output pins.  I put this in the version specific *hardware.h* files.
  * With the simplified display and reduced frequency spectrum size, the processing loop on the Prototyping System only takes about 10ms, about 1/7 the time taken to complete processing a loop with the RA8875 display.  Adding the waterfall added another 10 ms, or 20ms total.

### A new display driver for ST7796

  * I got the T41 software running on the ProtoSupplies Mini Platform with a 3.5", 480x320 resolution ST7796 display.  The display is an upgrade to the ILI9341 and draws only about 0.2 amps, about 0.1 amps less than the RA8875 display on the Project System, though that system also sports more hardware.

![displayST7796](https://github.com/tmr4/T41_Vxx/blob/dev/v0.01/images/T41_MiniPlatform.jpg)

* This display is a better candidate for the T41 mini than the ILI9341.  It's not clear yet though if the reduced current draw compensates for the reduced resolution.
* I've added a basic hardware version for the Mini Platform.  This version doesn't support a front panel, but that could be added.  There is a single audio circuit on the baseboard and the pins needed to add a second audio board like I did on the Project System aren't readily available.
* I don't plan more work on this development board for now with the introduction of the Audio Platform.

### The Audio Platform

* With the help of Ken from ProtoSupplies and Tim, another Teensy enthusiast, we got the Audio Platform working consistently.  Some boards require compiling to run at 600MHz rather than the normal 528MHz used by most T41 users.  This could reduce the longevity of the Teensy processor, but for use in the Audio Platform the risk shouldn't be significant and a board replacement is easy.
* The Audio Platform also only has a single audio circuit on the baseboard.  Adding a second doesn't look possible.
* I plan to alter the T41 audio configuration routines to accommodate a single audio board.  This will be useful in receiver only T41 designs.
* With that, I'll be able to add the Audio Platform as a version in T41_Vxx.

## Recent Work

### WSJT-X Support

* WSJT-X working over T41 USB connection
  * Modified the FT8 Data mode to pass audio back and forth with a PC over USB at a 44.1kHz sample rate
  * Modified the wsjt module CAT controls for transmit
  * T41 switches to FT8 Data mode upon receipt of *ID;* command
  * Calibration of FT8 Data mode still in progress

### Standalone FT8 Mode

* Standalone FT8 based on [ft8_lib](https://github.com/kgoba/ft8_lib)
  * a modified version of the library for the T41 is in the *ft8_lib* folder within *src* folder and available to all hardware versions
  * This mode is available with a special data mode, internal FT8 mode
  * The mode can also play wav files.
  * FT8 UI is mouse driven at present and requires PSRAM.

![T41 Internal FT8 contact](https://github.com/tmr4/T41_Vxx/blob/dev/v0.01/images/T41_ft8.jpg)

The standalone FT8 interface is limited due to the display size.  Currently three message lists are available, all scrollable with a mouse wheel.  To the left is a list of all recent FT8 messages.  In the middle is a list of all recent CQ messages.  To the right is a list of all messages around the selected FT8 receive frequency as entered at the bottom of the infomation box.  The bottom lines of the display show the current/most recent QSO.  The next FT8 message to be transmitted is shown in green in the last line.  Yellow messages are completed.  The line above the message lists shows the details of the selected FT8 message, which can be changed by clicking on a message in any of the lists.

### New display layout

  * captures some unused space, expanding the area for FT8 messages and more info box items

![T41 new display layout](https://github.com/tmr4/T41_Vxx/blob/dev/v0.01/images/T41_newDisplayLayout.jpg)

### New front panel box for Project System

  * organizes encoders and switch matrix making it easier to add these to development boards for testing

![T41 new display layout](https://github.com/tmr4/T41_Vxx/blob/dev/v0.01/images/T41_FrontPanelBox.jpg)

  * The Project system now sports two audio adapters, a microphone, two sets of earbuds to listen to RX/TX signals, four encoders and a switch matrix
  * An AD3 is connected to provide IQ input signals and examine TX output signals
  * A Digital Discovery is connected to examine process loop timing
