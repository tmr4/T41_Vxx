# Ongoing and Recent Work

## Ongoing Work

### Unifying the Calibration Code

I did some work automating the T41 calibration code a year ago. That code was hardware specific. I've never been happy with it because it breaks my unified code approach. First, very similar code exists for separate hardware versions. That's not a huge deal since most of it is in flash memory. But it increases the code base to keep updated. Second, the code relies on a specific display model and there is a lot of it for each hardware version.  All of this must be modified if another display model is used. That wouldn't be a fun project.

I like the calibration routines I came up with back then but it is impractical maintaining separate hardware versions with the changes I've made to the core code. I'm moving the calibration code to the core model with hardware specific code only as needed. I also intend to move away from display dependent routines. We'll see how that works out.

My new routines will be mostly automatic. Manual calibration, while interesting, isn't practical. I probably won't update those old routines to work with my new core code.

Lastly, I've done away with using the EEPROM. I started this to avoid the problem of testing alternative T41 software and the requirement to erase the EEPROM for a clean install. I've always hardcoded my desired operating parameters and calibration factors because of this and that's the direction I'm taking going forward. This might not work for people testing my software, but I haven't found it to be a problem.

### T41 Remote with WIFI

I've been experimenting with creating a WiFi connection between the T41 and a remote unit.  My early tests show that this is probably possible but will require special attention and maybe a custom board.

The first step is to connect the T41 to a WiFi module. I've done that successfully before with my T41 remote display which transferred the T41 display data to a remote display, but without audio. That needed a relatively low data rate.  I was able to do it over Bluetooth. Now, similar to my Ethernet remote connection, I wanted to include audio. That requires a faster WiFi connection.

A 512-byte block of IQ data is produced by the T41 every 667 microseconds. That needs transferred from the T41 Teensy to the WiFi module and then sent out over UDP WiFi.  SPI is the only viable transfer mechanism between the Teensy and Wifi module at that rate.

For this test, I'm focusing on the v11 T41. SPI1 is available on the Teensy but not readily accessible from the main board. The v11 T41 main board breaks out eight unused pin where headers can be added. From these pins, FlexIO3 can be used to create a virtual SPI port for the transfer. FlexIO SPI has the advantage that it can be run at a faster clock rate than a standard SPI port. FlexIO3 has the disadvantage of not supporting DMA. That means the Teensy processor has to drive the data transfer.

Most of the ProtoSupplies Teensy development boards come with a WiFi module. For the T41 portion of these tests, I mocked up the transfer of IQ data from a Teensy 4.1 to the WiFi module on my ProtoSupplies Prototyping System for Teensy board. The Prototyping System uses an ESP32-S WiFi module. The standard SPI port on that board should be used for best performance. However you have to run in slave mode which the standard Arduino library doesn’t support.

My first test was to see how fast I could transfer a data block between the Teensy and ESP32 module. Theoretically, the Teensy can run a FlexIO SPI bus up to 200 MHz, but speeds above 50 MHz probably require special hardware. The absolute slowest clock speed possible is about 12 MHz, reserving half of the 667-microsecond interval for the WiFi transfer (2/667us*4096bits).

The FlexIO3 SPI speed with the Teensyduino FlexIO library seems to default to a maximum of 15 MHz. At that rate it takes about 273 microseconds to transfer one data block. The ESP32-S module had no problem with transfers at this rate. Early tests showed that the UDP WiFi transfer would take a similar amount of time. Thus the combined data transmission, Teensy to WiFi, fit well within the required transfer window with about 100 microseconds to spare. Of course, it would be nice to have more headroom as I’d like to pass CAT commands back and forth as well.

Next, I tried to see how far I could push the clock. No matter what I tried I couldn’t get past 15 MHz on the FlexIO3 SPI port. After a morning with AI I found a feature (bug?) in the FlexIO library that prevented higher clock speeds. A simple mod allowed faster rates, allowing me to test the limits of this transfer. The transfer was successful at 30 MHz. Dropped packets occurred at 40 MHz.

The 137 microsecond IQ data transfer time over SPI at the 30 MHz clock was attractive. However, I started getting data errors when I turned on the WiFi module. These errors persisted at slower SPI clock rates, even with a longer transfer window. Errors seemed to clear up with a 1MHz SPI clock.

This points to problems with my hardware set up, not surprising since I just used jumper wires to connect the Teensy and ESP32. I think the setup would be ok with a shielded connection or custom PCB, both at the T41 and remote. Unfortunately, I’m not into creating that type of hardware so I won’t be taking this further.

### Ethernet, T41 Operation, Loop Flow and Timing

The addition of a remote connection, especially Ethernet, added to the loop complexity. Sprinkled through the code and some online posts are some discussions of T41_Vxx operation, loop flow and timing. The loop I'm referring to is everything that occurs between the start and finish of the main loop. This section provides some historical context and an overview how adding an Ethernet connection changed things.

The loop timing during RX of the original T41 code was driven by the time required to update the display, primarily rendering the frequency and audio spectrums and the waterfall. For best operation, audio and some I/O processing were handled from the main display update routine. This linked operation of the radio to updating the display each loop meaning that some operations, full screen menus for example, silenced the radio. The entire TX operation on the otherhand occurs within a single loop. The T41EEE software and an early version of the v12 software use this approach.

In the current v12 software, Phoenix, each loop processes 16 blocks of audio data, either TX or RX, and a slice of waiting I/O interactions and display updates. The size of the slice of the non-audio operations is set to maintain artifact free audio. Thus it takes many loops to render one frame of the display or respond to multiple I/O events.

The T41_Vxx software operates somewhere between these two. Audio processing in T41_Vxx is no longer driven by display updates. Rather the main display update routines yield to audio processing. This means that the radio can operate without updating the display or even without a dispaly at all. As with the original software, the time to complete a loop in RX is the time to render one frame of the display. As with the original software, an entire TX operation occurs within a single loop.

Adding an Ethernet connection makes this review useful because processing the IQ data required another yield function. Ethernet processing needs to be done approximately every 667 microseconds to ensure smooth data flow. This can't be done from an ISR due to the Ethernet library design. It also can't simply be added to the normal audio processing yield function as that doesn't occur frequently enough. In fact, the audio processing routine calls the Ethernet yield at several points to maintain good Ethernet traffic flow.  With this, a Phoenix-style slice-of approach won't work unless you start slicing the audio processing as well. The trade off? I have to examine longer running processes and sprinkle yields at appropriate places to maintain good IQ data flow. So far, outside of audio processing, the only place I've had to put another Ethernet yield is in the RA8875 wait while busy routine called during the waterfall move.

### Reworked Radio Role and Remote Communication

My use of RADIO_ROLE to specify the supported modes of remote communication was getting overly complicated. I thought having highly configurable options was needed to conserve memory for other features, but this isn't really needed with the addition of PSRAM. Also, code readablility and the functionality of other operating modes, standalone and plug and play in particular, suffered as I've focused exclusively on WSJT-X remote operation with Ethernet recently.

Given this, RADIO_ROLE now simply specifies whether the device is a T41 or remote and I'm going to include plug and play Ethernet as part of the core code. That means the only selectable remote operation modes at compile time are 1) WSJT-X audio/CAT over USB or 2) remote operation over USB. These are mutually exclusive. If I see these impacting performance much, I may revisit this in the future and include these as hardware modules for drop in activation.

As part of making a bidirectional IQ data path between the T41 and remote unit, I've restructured many of the classes previously discussed, splitting some up to avoid duplication. This simiplified many of the classes, but added to the initialization effort. I may revisit this.

Here is a brief overview of the classes involved in T41/remote communication.

Communication and Support Classes:
 * CatControl - processes CAT commands (the file remoteRadio.cpp - provides the specific CAT commands for the T41 remote and WSJT-X operation)
 * ConnectBase - provides generic connection methods; the Audio and Ethernet classes derive from this class to provide a common base class link to ConnectManager
 * ConnectManager - manages the state of the connection between the T41 and remote
 * EnableBase - provides begin/end/enabled capability; the Audio objects derive from this classs to provide a common base class link to audio config routines
 * EthernetQueue - queues 2-channels to/from related AudioStreams and a UDP data port
 * TCPClient - manages Ethernet TCP connection; derives from ConnectBase to provide link to CatControl
 * TCPServer - manages Ethernet TCP connection; derives from ConnectBase to provide link to CatControl
 * T41Properties - links T41 properties to their display update method and CAT command; properties derive from ReadOnlyProperty and Property which derive from T41Update which provides the link to CatControl

The following classes derive from AudioStream to stream IQ data to/from other Audio library objects:
 * AudioOutputToQueue - streams to EthernetQueue
 * AudioInputFromQueue - streams from EthernetQueue

The above combined with EthernetQueue create a zero-copy T41/remote IQ data transfer.

Here's the updated communication diagram (only an Ethernet link can be used since WSJT-X uses the remote unit Serial connection):

```
                                                    comm path
                                                  Ethernet Only
                                            T41 <---------------> Remote
                     CatControl <-> TCPServer   <- CAT command ->   TCPClient <-> CatControl
i2s_quadIn  --> AudioOutput --> EthernetQueue   -- RX IQ data -->   EthernetQueue --> AudioInput  --> Q_in_L/R
i2s_quadOut <-- AudioInput  <-- EthernetQueue   <- TX IQ data ---   EthernetQueue <-- AudioOutput <-- Q_out_L/R_Ex
                                            T41 <---------------> Remote
```

### Adding WSJT-X TX Capability to Remote Unit

Running WSJT-X with audio and CAT control over USB from the remote unit is fairly easy for RX and passing the CAT commands back to the T41.  All that's left then for full WSJT-X ops from the remote is passing the FT8 TX signal back to the T41. That's best handled by processing the TX signal to IQ data in the remote and passing that back to the T41. This mirrors the RX IQ data the T41 passes to the remote.

A wrinkle is that currently the T41 and remote each handle just one half of the operation, either transmission or reception of the RX IQ data. Passing WSJT-X TX IQ data back to the T41 from the remote requires that both units have both transmission and reception capability.

Here's the updated communication diagram (only an Ethernet link can be used since WSJT-X uses the remote unit Serial connection):

```
                                comm path
                              Ethernet Only
                        T41 <---------------> Remote
CatControl  <-> AudioOutput <- CAT command -> AudioInput  <-> CatControl
i2s_quadIn  --> AudioOutput -- RX IQ data --> AudioInput  --> Q_in_L/R
i2s_quadOut <-- AudioInput  <- TX IQ data --- AudioOutput <-- Q_out_L/R_Ex
                        T41 <---------------> Remote
```

During standalone or WSJT-X ops, *Q_out_L/R_Ex* in the T41 are routed to *i2s_quadOut*. This remains the same when WSJT-X is connected to the remote but *i2s_quadOut* is feed directly from AudioInput, as shown above instead of being driven by *PlayExciterIQData* which now happens in the remote. That's similar on the RX side on the remote where AudioInput feeds directly into *Q_in_L/R* to be used in DSP.

It's easy on the remote side as well. There *Q_out_L/R_Ex* are connected to AudioOutput and the streaming occurs automatically when they're played in *PlayExciterIQData*. Thus the only change in core DSP software is a modified audio configuration.

The last wrinkle is making the AudioInput and AudioOutput objects work together in one build. Each is designed as standalone and works with the CAT control and connection manager classes to manage the CAT command and IQ data flow. There's no need to recreate that framework just to send TX IQ data back to the T41, but some refactoring is needed to make things efficient. I'll tackle that next.

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
