# T41_Vxx

This is a combination of my T41 v11 and v12 projects, creating a consistent code base for my T41 projects.  The combinations should make it easier to maintain consistency between the two versions and allow me to easily add new versions.

The driver for this project was to leverage all of the work I put into adding features to my v11 radio such as:

* new input capability (mouse and keyboard)
* new modes (NFM demodulation and some data modes)
* new features (beacon monitor, CW message keyer, CAT control, remote display, USB host connection to another T41)

![T41 new display layout](https://github.com/tmr4/T41_Vxx/blob/dev/v0.01/images/T41_newDisplayLayout.jpg)

## Ongoing and Recent Work

See [Ongoing and Recent Work](https://github.com/tmr4/T41_Vxx/blob/dev/v0.01/RecentWork.md).

## Use

This software supports several versions of T41 hardware. To customize the software for specific hardware version, copy the files from the desired hardware subfolder (in the *T41_SDR/hardware* folder) and paste them into the *T41_SDR/src* folder.

Support for the T41 display is handled differently. To add support for a specific display, copy the folder for the desired display (in the *T41_SDR/hardware/diplays* folder) and paste the folder into the *T41_SDR/src* folder.

When managing the project, it's best to keep the *src* folder free of hardware specific files when committing changes.  Just copy any changed hardware files back to their specific hardware version folders, delete the hardware files in the *src* folder and proceed as normal.  Note that the *ft8_lib* folder should remain in the *src* folder.

For more detail see [Accomodating Hardware Differences](https://github.com/tmr4/T41_Vxx/blob/dev/v0.01/hardware.md).

### Caution

I've disabled saving operating parameter in EEPROM to facilitate changing versions.

This is a work in progress.  Some functions may be broken and will likely remain so until they are of interest to me.  This is especially true for the v12 radio. I'm still building that so I've yet to add a lot of functionality.  Use at your own risk.

## Pin Usage

See [pinout](https://github.com/tmr4/T41_Vxx/blob/dev/v0.01/Pinout.md).

## Features Available to All Hardware Versions

These features are display dependent and are fully supported with the RA8875 display.  They haven't been implimented for the other displays.

### Mouse

Adds mouse support to models with a USB host connection.   Currently the mouse can be used as follows:

* Within the menu area (bottom of waterfall):
  * Open menu with right click
  * Scroll through menu options with the mouse wheel
  * Select menu option with left click
  * Adjust entry value with mouse wheel

* Within the frequency area:
  * Adjust the frequency in deciles with the mouse wheel
  * Zero out the lower portion of the frequency with a right click
  * Switch VFO with a left click on non-active VFO

* Within the the operating stats area:
  * Center the frequency with a left click on the center frequency
  * Change the band up or down with a left or right click on the band indicator
  * Toggle through mode and demodulation with a left click on the respective item

* Within the the audio spectrum box:
  * Adjust the audio filter width with the mouse wheel
  * Toggle active filter end with a left click

* Within the the spectrum/waterfall:
  * Set the frequency with a left mouse click
  * Adjust the frequency up or down by the active increment with the mouse wheel

* Within the the info box:
  * Change the volume up or down with the mouse wheel
  * Rotate up or down through frequency increments with the mouse wheel
  * Select the active frequency increment with a left click (label of active increment is highlighted in green)
  * Adjust the zoom level with left or right click or mouse wheel
  * Toggle the noise floor setting with a left click on the NF Set item
  * Adjust the noise floor with the mouse wheel when NF Set is on

* Within the the Data mode message area:
  * Select active message with a left click
  * Cycle through messages with mouse wheel

### Keyboard

Adds optional keyboard support to a T41 with a USB host connection. It uses about 7k RAM.  Keyboard connects to the Teensy USB Host connection.

### Morse code keyer

Adds a keyboard/memory CW keyer.  It currently requires the keyboard feature but could be simplified to be used on a standalone T41.  It has the following features:

* Set up to 10 preset messages in code.  The selected preset message is highlighted in green at the bottom of the info box.
* Select desired message to transmit by using the left/right arrow keys on keyboard.
* Transmit selected preset message by pressing enter key.
* Press the insert key to edit the selected message. The color of the selected message will change to yellow to indicate that you are in edit mode.  The cursor is at the end of the message.  You can add to the message by typing.  You can overwrite the message using the backspace key.  To keep any changes, use the enter key to exit the edit mode.  If you want to discard the changes, press the escape key.  Any changes made are lost after you turn off your T41.  Note that the normal keyer navigation keys are not active in edit mode.
* Enter keyboard mode by pressing the up/down arrow key on the keyboard.  The preset message color changes to white to indicate it's no longer active.  You can return to the preset message with the up/down arrow key on the keyboard.
* Type a message on the keyboard to create a custom CW message. It will be highlighted in green indicating the keyboard mode is active. Press the enter key to send the message or the escape key to erase it.
* WPM and Sidetone volume are adjustable with the T41 CW WPM and Sidetone menu items.
* The keyer produces fairly accurately timed CW at 15 WPM.  It's been successfully decoded with another radio.  I need to verify the timing at other WPM rates.  I may need to do some adjustments to the timing.
* CW signals (dit/dah) are shaped with a 5 ms raised cosine at the start/end to reduce bandwidth and minimize key clicks.
* Note: I scaled the keyer sidetone volume to give a comfortable volume over the entire RF power range at a setting of 20.  This isn't consistent with the current T41 sidetone volume, so if you've increased that you'll want to reduce it before you try the keyer.

### Live noise floor

The noise floor button toggles between Off, Auto and On.  When set to On you can adjust the noise floor with the Filter/Menu/Change encoder live while the radio is operating.  This noise floor setting for each band is preserved to the EEPROM when the noise floor button is toggled back to Off.  When set to Auto, the T41 will maintain the noise floor at the bottom of the frequency display.  This auto noise floor setting is not preserved.  Requires configuration setting USE_LIVE_NOISE_FLOOR set to 1.

### Narrow-band FM

Adds narrow-band FM demodulation. Adds separate, adjustable demodulation filter in addition to adjustable filter for audio output.  Filter button toggles between NFM demod filter and the high and low audio spectrum filters.  The active filter line is highlighted in green.

### T41 beacon monitor

![Beacon Monitor](https://github.com/tmr4/T41_Vxx/blob/dev/v0.01/images/beacon_monitor.jpg)

* a world map view with the SNR for each frequency located in a small colored square below the call sign for each beacon location (image below with random SNR values to show effect).  I've only show three bands above but all five bands could be displayed with a bit of fiddling.  This view includes some monitor info at the bottom left of the display (band and beacon currently being monitored and the audio volume).  The information, including any volume changes, is currently only updated every 10 seconds. There is room for other information.
* the beacon monitor can be accessed via button 18 (sorry Bearing map!) or the *Beacon Monitor* menu item.  You can exit the Beacon Monitor by pressing button 18 again or via the menu.
* Notes:
  * world view bitmap for beacon monitor is in the images folder.
  * an accurate clock is assumed.  More work could be done in this area.
  * The SNR calculation needs work.  Aging SNRs in some way could be an option.
  * Bill has described a fairly minimal beacon receiver.  The addition of a USB host connection would allow mouse input which opens many options for user interaction.
  * This version *completes(?)* the transition to an operating T41 radio without normal screen updates. It's still a work in progress as there might be other areas of code that I need to *discover* still updating the screen.
* The following alternate display options are coded but not fully developed or exposed for user selection:
  * prints the following to the T41 display:
    * the active beacon for each frequency (1st column)
    * a rolling list of beacons showing the calculated SNR for 20m, 15m and 10m (2nd column)
  * an azimuthal map showing the bearing to the currently active beacons
  * a world map view with the call sign highlighted according to the SNR for the frequency shown (cycles through the five bands, one every 10 seconds)

![Beacon Monitor with random SNR](https://github.com/tmr4/T41_Vxx/blob/dev/v0.01/images/bm_random_snr.jpg)

## Other T41 Related Apps

See [Other apps](https://github.com/tmr4/T41_Vxx/blob/dev/v0.01/OtherT41Apps.md)

  * PC beacon monitor
  * PC control
  * PC debug window
  * Set T41 clock
  * Interconnect T41 radios
  * Remote display
