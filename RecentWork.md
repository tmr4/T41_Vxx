# Ongoing and Recent Work

## Ongoing Work

### CAT Communication Spy

Running WSJT-X with audio and CAT control over USB has one drawback: the only Teensy serial port available is taken by WSJT-X. This makes it difficult to debug the CAT communications traffic. I added a simple display routine to show the comm traffic in the lower portion of the waterfall area.

![catSpy](https://github.com/tmr4/T41_Vxx/blob/main/images/catSpy.jpg)

The image shows the WSJT-X startup communications with the rig set to Kenwood TS-890s. The text in white are WSJT-X CAT commands to the T41. The text in green are the T41 replies. WSJT-X is pretty forgiving in the responses received when establishing a connection. What's absolutely required are proper responses to the ID and FA commands. You can see that WSJT-X changes the frequency a small amount during the startup sequence to verify that it has control of the frequency.

After a successful connection, WSJT-X will poll the unit at the frequency specified in Settings->Radio->Poll Interval. For this rig, depending on your settings, WSJT-X also uses the TX, RX, and SP commands during normal operation. The commands used with other rigs may differ.


***BTW -*** you might notice non-standard values for the Heap, Stack, and Load items in the info box. For these items, I cycle through various indicators to keep better track on Teensy resource use. The Heap item cycles through heap memory available, audio memory blocks currently in use, maximum audio memory blocks used since program start, and a dropped audio block warning. The Stack item cycles through the stack available (white), max stack used since last time (green), and max stack used since program start (yellow). The load item shows the traditional DSP load and the average frames per second display update rate.

### Plug and Play Remote Unit Connections

So far I've used special compile options to determine the connection type to a remote unit.  That's probably how the software will normally be compiled and run as it keeps unwanted capability from crowding the Teensy memory. However, there could be times that you'd like to connect your T41 to a remote unit over either USB or Ethernet and don't want to reload the software for each connection type. Plug and Play Remote Connections to the rescue. This capability is available by selecting the plug and play radio role for the units in the hardware configuration file (*hardwareConfig.h*).

The plug and play set up is working well except for an occasional glitch where the T41 will crash when the USB cable is disconnected and reconnected. The crash occurs because a pending/partial data transfer when the cable is pulled attempts to finish when the cable is reconnected. In that process, a null pointer related to the old connection is used, causing the crash.

The only way I could solve the problem was modifying the USBHost library. I documented the problem in a [PJRC forum post](https://forum.pjrc.com/index.php?threads/usb-host-serial-crash-on-cable-disconnect-reconnect.77973/). Maybe this will lead to a Teensyduino revision. If not, my version of the T41 code will have another modified library.

### Restructured Remote Communications

As part of my remote unit work, I've restructured all of the code involved in T41 remote communications. Here is a brief overview of the new classes involved.

Communication and Support Classes:
 * CatControl - processes CAT commands (the file remoteRadio.cpp - provides the specific CAT commands for the T41 remote and WSJT-X operation)
 * ConnectManager - manages the state of the connection between the T41 and remote
 * ConnectBase - provides generic connection methods; the Audio classes derive from this class
 * USBManager - contains the global USBHost and USBHub objects
 * TCPServer - manages Ethernet TCP connection; member of AudioOutputEthernet
 * TCPClient - manages Ethernet TCP connection; member of AudioInputEthernet
 * T41Properties - links T41 properties to their display update method and CAT command; derive from template classes ReadOnlyProperty and Property which derive from T41Update which provides the link to CatControl

The following classes stream IQ data and support the CAT connection between the T41 and remote:
 * AudioOutputEthernet - IQ data over UDP and CAT over TCP
 * AudioOutputHostSerial - USB host serial
 * AudioInputEthernet - IQ data over UDP and CAT over TCP
 * AudioInputSerial1 - Dual Serial, alias for template class AudioInputSerialT specialized for this mode

```
                               comm path
                            USB or Ethernet
                       T41 <---------------> Remote
CatControl <-> AudioOutput <- CAT command -> AudioInput <-> CatControl
i2s_quadIn --> AudioOutput ---- IQ data ---> AudioInput --> Q_in_L and Q_in_R
                       T41 <---------------> Remote
```

### Streaming IQ Data to a Remote Unit

I've long wanted to pass audio to a remote unit.  This wasn't possible with my previous wireless remote. The Bluetooth I used to transfer T41 data just wasn't fast enough.  With the T41 Desktop Companion (Audio Platform, AP hardware version) I decided to try out USB transfer. That proved successful.

There are a lot of different ways to approach this.  I've tried many over the last week.  The key with all of them was that the T41 had to feed data to USB Host serial only when it could take a full packet (512-bytes) and the remote had to read a packet from USB serial when it was available.  Failing this results in USB buffer issues and system instability.

The standard way to address this is double buffering. But this adds overhead and while successful, slows down both the T41 and remote unit. In my early tests I found that the T41 required significantly more time to transmit 16 blocks of IQ data (one display frame) than the remote needed to read it. To accommodate the difference, I increaced the USB Host buffer to hold 16 blocks of IQ data and let the remote drain this as fast as it could.  That wasn't a good long-term strategy as I didn't want to modify Teensyduino or use such a large buffer needlessly.

I tried various other buffering/syncing strategies to increase throughput. These required custom code for buffering and syncing the streaming data between the units. I even tried turning off display updates on the T41 which isn't really needed for remote operations, but isn't great if you're using the remote unit as a desktop companion to the T41.

I ultimately settled on creating Teensy Audio library USB serial objects.  The T41 object takes input directly from the I2S IQ input object and streams the data to USB Host serial.  The remote object takes input directly from USB serial and streams the data to the Q_in_L/R input queues ready for the normal RX processing. The streaming is handled in the background by the Teensy Audio library.

You can find these objects in *input_usb.h* and *output_usb.h*.  These object are very light, reading/writing directly from/to their connected objects with no buffer in between. The background work by the Audio library makes this run smoothly. The objects could be made more reliable with double buffering and syncing, but this hasn't proved necessary in my testing.  I'll only consider it if I notice the units get out of sync during normal operation.

I've also worked out streaming IQ data over TCP. They are Audio library objects similar to the USB objects. You can find them in *input_tcp.h* and *output_tcp.h*. The main wrinkle in these objects is that Ethernet related calls can't be made from an interrupt state. That means we can't take advantage of the Audio Stream update function to drive the input/output, but the object's read/write functions must be called frequently to drive the data flow. The update function either fills a buffer from Audio Stream objects for sending data to the TCP port (output_tcp.h) or empties a buffer filled from a TCP port to the connected Audio Stream objects (input_tcp.h). The major advantage with these objects is that they can be connected to a local network.  Here is an Ethernet enabled Audio Platform acting as a *T41 Bedside Companion*, connected to a local network in my bedroom.  The T41, with a test signal, is at my workbench connected to the local network there. The four encoders serve the same purpose as on the T41. Most other options are available with the attached mouse.  Actions on this unit are duplicated on the T41 and visa-versa. I haven't worked out TX on this yet, but it should be possible.

![Desktop Companion](https://github.com/tmr4/T41_Vxx/blob/main/images/T41_AP_Bedside.png)


### Global Working Variables to Properties

* I've been slowly converting the T41 global working variables to C# style properties.  These can notify remote to take action or cause the display to be updated.  This should eliminate having such things spread throughout the code.

### Refined hardware and new display drivers

* I've extracted much of the T41 hardware specific code into separate version specific hardware *libraries*.  The front panel and display are two areas I haven't previously attempted, for different reasons.  My original thought was to make the front panel code applicable to all versions with a group of defines activating the correct code depending on the selected front panel. This created confusing code and while seemingly practical, was never used in practice as my radios all have a version specific front panel.  It also made it more difficult to develop code for the various development boards I've been working with.
* I've completed the basic RA8875 display module and tested against the v11 and Project System hardware.  The Project System was the easiest hardware version to develop for as I haven't defined calibration code for it.  I also created a no display version that removes the body of required display functions.
* More work is needed to accomodate the calibration routines.  These are highly display specific as well as hardware specific to some extent.
* I've tackled the audio hardware to accommodate different hardware, both the hardwired ADC/DAC in v11/v12 and other audio adapter setups including an RX only version.

### A new display driver for ILI9341

* As a proof of concept, I developed a basic display module a Teensy 4.1 Prototyping System from ProtoSupplies.  That system uses a 3.2" 320x240 ILI9341 display.
* Teensyduino supports this display, so modifying the RA8875 display module code for this display was straighforward.  Some display function used in the RA8875 module aren't supported by the ILI9341, font size functions for example.  The code needed reworked for those.
* This mock up for the ILI9341 display highlighted that much of the display code is customized for the RA8875, not just based on it's resolution, but the particular placement of elements on the display.  Many of those need reworked when moving to a new display.

![displayILI9341](https://github.com/tmr4/T41_Vxx/blob/main/images/T41_altDisplay_PrototypingSystem.jpg)

  * Adding a small waterfall took more work as the display doesn't have a move function.  The entire waterfall needs to be written to the display line by line each processing loop.  A circular buffer works well for this.
  * I have 2.4" and 2.8" versions of this display as well.  I thought the smaller displays would draw less current, but that's not the case.  The smallest display drew the most current and while it's a bit of an apple/oranges comparison, it drew almost as much as my v11 T41 main board with 5" RA8875 display.  Even the more efficient 3.2" version was less than 50 mamps less than that combination.  These displays aren't going to be much of a battery saver on a T41 Mini.
  * The spectrum update on the ILI9341 is quite snappy.  As with all mockups on development boards, the trickiest thing is defining the Teensy pin assignments so they don't conflict with ones being used by the development board.  The switch matrix busy pin threw me for a while with the Prototyping System.  This has taught me to have all of the Teensy pin assignment located in one place and separated by input/output pins.  I put this in the version specific *hardware.h* files.
  * With the simplified display and reduced frequency spectrum size, the processing loop on the Prototyping System only takes about 10ms, about 1/7 the time taken to complete processing a loop with the RA8875 display.  Adding the waterfall added another 10 ms, or 20ms total.

### A new display driver for ST7796

  * I got the T41 software running on the ProtoSupplies Mini Platform with a 3.5", 480x320 resolution ST7796 display.  The display is an upgrade to the ILI9341 and draws only about 0.2 amps, about 0.1 amps less than the RA8875 display on the Project System, though that system also sports more hardware.

![displayST7796](https://github.com/tmr4/T41_Vxx/blob/main/images/T41MiniPlatform.jpg)

* This display is a better candidate for the T41 mini than the ILI9341.  It's not clear yet though if the reduced current draw compensates for the reduced resolution.
* I've added a basic hardware version for the Mini Platform.  This version doesn't support a front panel, but that could be added.  There is a single audio circuit on the baseboard and the pins needed to add a second audio board like I did on the Project System aren't readily available.
* I don't plan more work on this development board for now with the introduction of the Audio Platform.

### The Audio Platform

* I am developing the ProtoSupplies Audio Platform as a desktop companion for the T41.  The platform sports 4 encoders, a 7" RA8875 display with touchscreen and breaks out about a dozen unused Teensy pins.  It's just about perfect for this role.

![AudioPlatform](https://github.com/tmr4/T41_Vxx/blob/main/images/T41AudioPlatform.jpg)

* My plan is for this to be a user interface for the T41 with I/O sent back and forth with the T41 over USB.  Currently this works with the 4 encoders (from left to right: volume, filter, fine tune, center tune).  I'm working on passing other I/O.
* With the help of Ken from ProtoSupplies and Tim, another Teensy enthusiast, we got the Audio Platform working consistently with the T41 software.  Some boards require compiling to run at 600MHz rather than the normal 528MHz used by most T41 users.  This could reduce the longevity of the Teensy processor, but for use in the Audio Platform the risk shouldn't be significant and a board replacement is easy in that unit.
* The Audio Platform only has a single audio circuit on the baseboard.  Adding a second doesn't look possible.
* I've altered the T41 audio configuration routines to accommodate a single audio board.  This will be useful in receiver only T41 designs.
* The Audio Platform is now included as a hardware version in T41_Vxx.
* Unfortunately, the Arduino IDE and/or Windows is a bit fussy when trying to successively compile for different hardware versions. You can't simply replace the hardware files in the *src* folder, change the Teensy in the IDE and compile. The system seems to lose one or the other Teensy connection and you have to disconnect the Teensy.  Sometimes, the Teensy also must be reset before you can continue. I've resolved this by only connecting one hardware setup to the PC at a time and deleting the previous IDE working folder for the sketch.  Needs more testing. What seems to work best when switching between systems is to connect the system of interest while the other system is still connected and then disconnecting the second system.  This might just be a Windows thing.  I think part of the problem is that the IDE is reusing object files from the previous compile even though cpu speed is changed. You'd think that these problems wouldn't exist when changing cpu speeds, like when working with an AP/PS combo, as a full recompile should be needed. But perhaps the IDE isn't tracking that.
* More research/testing with the above issue. Seems as if Windows doesn't stay sync'd with Arduino and/or Teensyduino and excessive port reservations are made. These seem hard to delete in regedit so I successively attached the various Teensy boards I've used and unintalled the device in Device Manage/Ports. Then I was able to switch back and forth between connected hardware within the IDE.  To be safe I've been disconnecting the serial monitor and deleting the sketch folder.
* Keeping the IDE serial monitor disconnected appears to prevent the compilation problem noted above.  I am occassionally seeing other apps close (VSCode) or have problems (Waveforms) on compile.
* I'm seeing an occasional slowdown in the AP display. I've never noticed this with any of the other hardware versions.  This could be platform specific or for the particular Teensy board being used (the one in the system now requires 600MHz to operate properly).

## Other Recent Work

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

![T41 Internal FT8 contact](https://github.com/tmr4/T41_Vxx/blob/main/images/T41_ft8.jpg)

The standalone FT8 interface is limited due to the display size.  Currently three message lists are available, all scrollable with a mouse wheel.  To the left is a list of all recent FT8 messages.  In the middle is a list of all recent CQ messages.  To the right is a list of all messages around the selected FT8 receive frequency as entered at the bottom of the infomation box.  The bottom lines of the display show the current/most recent QSO.  The next FT8 message to be transmitted is shown in green in the last line.  Yellow messages are completed.  The line above the message lists shows the details of the selected FT8 message, which can be changed by clicking on a message in any of the lists.

### New display layout

  * captures some unused space, expanding the area for FT8 messages and more info box items

![T41 new display layout](https://github.com/tmr4/T41_Vxx/blob/main/images/T41_newDisplayLayout.jpg)

### New front panel box for Project System

  * organizes encoders and switch matrix making it easier to add these to development boards for testing

![T41 new display layout](https://github.com/tmr4/T41_Vxx/blob/main/images/T41_FrontPanelBox.jpg)

  * The Project system now sports two audio adapters, a microphone, two sets of earbuds to listen to RX/TX signals, four encoders and a switch matrix
  * An AD3 is connected to provide IQ input signals and examine TX output signals
  * A Digital Discovery is connected to examine process loop timing
