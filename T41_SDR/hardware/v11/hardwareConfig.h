// v11 specific hardware config file

#define VERSION "v11_dev/v0.04" // up to 14 characters can fit *** can adjust infobox ShowVersion routine to accommodate more if needed ***
#define RADIO_ID 01               // 0-999, radio ID is reported as 24 to WSJT-X (Kenwood TS-890S)

#define RADIO_ROLE 0              // 0: DEVICE_ROLE_T41, or 1: DEVICE_ROLE_REMOTE

/*
  Remote operation:
  T41_Vxx supports passing CAT/audio between a T41 and a remote device, such as WSJT-X, other PC app, or
  other T41_Vxx enabled devices. Enable the type of connections desired between the T41 and remote
  device below. WSJT-X can be run from either the T41 or remote, but not both at once. WSJT-X from
  the remote requires an Ethernet connection (USB connection not possible as the Serial port is used).

  *** plug and play Ethernet is part of the core code base and cannot be disabled; it does add some
      overhead which can be minimized by not having a remote unit connected via Ethernet ***

  Connection types:
    WSJT_USB_CAT_AUDIO
      For passing CAT/audio back and forth with WSJT-X over USB Serial at 44.1kHz sample rate in FT8 mode
      Compile with an Audio option selected, such as "Serial + MIDI + Audio"
      Can't be used with USB_ENABLED when RADIO_ROLE = remote

    USB_ENABLED
      For passing CAT/audio back and forth with remote over USB
      USB cable from the USB Host connection on T41 to USB Serial connection on remote unit
      Can't be used with WSJT_USB_CAT_AUDIO when RADIO_ROLE = remote

    CAT_ONLY
      Specifies what to transfer between T41 and remote
      false - both IQ data (audio) and CAT commands
      true  - only CAT commands

  *** see Connection Option Summary below for more detail ***
*/
#define WSJT_USB_CAT_AUDIO  false
#define USB_ENABLED         false
#define CAT_ONLY            false // not yet active

#define PROFILER_ACTIVE     false
#define CAT_SPY             false           // set to true to examine CAT communication traffic in waterfall area

#define MASTER_CLK_MULT 4ULL // FOURSQRP QSD frontend requires 4x clock

// pick one of the following display configurations
#define DISPLAY_LANDSCAPE
//#define DISPLAY_FLIPPED

#define READ_CENTERTUNE_ENCODER true // set to false if interrupt driven

// set to true for mouse/keyboard support
// uses about 44k of stack
#define HOST_KEYBOARD_MOUSE_SUPPORT false

//#define USE_BUFFERED_FT8_WAV // buffered wav file used for internal FT8 testing

#define USE_BPF_BOARD

//#define T41_REMOTE_DISPLAY

/*
  Connection Option Summary:

  This section describes how to configure the T41 and remote device for remote operation and specifies
  the compiler options required for each. The limitations of such operation are also discussed. The type
  of connection desired between the T41 and remote device may require compiling the software with certain
  compiler options and these might preclude certain T41 capabilities.  For example, WSJT-X w/ USB Audio/CAT
  control requires the "Serial + MIDI + Audio" USB Type compile option. There is only one USB serial object
  available with USB audio enabled.  This Serial object is reserved for WSJT-X use.  Any other use could
  disrupt WSJT-X control of the T41.

  The following minimum USB Type compile option is required for each operating mode:
    Mode                           T41                      Remote
    Standalone                    Serial                     na
    WSJT-X (USB)*                 Serial + MIDI + Audio      na
    Remote (USB)**                Serial                     Dual Serial
    Remote (Ethernet)**           Serial                     Serial
    Remote (USB or Ethernet)***   Serial                     Dual Serial
    Auto Cal (USB)                Serial                     Serial
    CAT only (Ethernet)           Serial                     Serial
    CAT only (USB)                Serial                     Serial
    WSJT-X (USB from remote)*     Serial                     Serial + MIDI + Audio

    *   configure WSJT-X for the serial COM assiciated with the T41/remote and 44.1kHz audio
    **  remote operation includes CAT control and high-speed IQ data transfer for spectrum display and audio
    *** connection is plug and play

  To enable remote operation, set RADIO_ROLE to the role of this device and enable the desired connection types,
  compile and upload. Likewise, set the role of the companion device and connection types, compile again and
  upload to that device. Connect the two devices with the selected cable type. Communication between the two
  devices should commence once the connection is established.

  The device is then configured as follows:

  T41 connected to a PC running WSJT-X (RADIO_ROLE = 1)
    - USB cable from the USB serial connection on this unit (T41) to a USB connection on the PC
    - Compile with an Audio option selected, such as "Serial + MIDI + Audio"

  Remote device connected to a T41 (RADIO_ROLE = 2)
    - USB cable from the USB serial connection on this device (remote) to the USB host connection on the T41
    - Compile with at least "Dual Serial" selected

  T41 connected to a remote devise (RADIO_ROLE = 3)
    - USB cable from the USB host connection on this unit (T41) to the USB serial connection on the remote

  For operation with a remote device, RADIO_ROLE equals 2 or 3, CAT control and Audio transfer
  is assumed. Audio transfer may be disabled in a future update.
*/

// The noted serial objects below are automatically assigned according to compile options for enabled services.
// For use with PC apps and connecting to other CAT controlled units over USB serial or the USB host connector.
//   Serial:         compiling with a single serial object with multiple services are enabled. Arduino IDE must be closed to connect to PC apps.
//   SerialUSB1:     compiling with Dual or Triple Serial (need to figure which COM port is associated with each)
//   SerialUSB2:     compiling with Triple Serial with two or more services enabled (need to figure which COM port is associated with each)
//   usbHostSerial:  for CAT control with another radio over USB host (not for use with PC)
//   usbHostSerial1: for passing audio to another radio over USB host (not for use with PC)
//
// Notes:
// All services are disabled (set to Serial). Any messages from/to these services are sent to Arduino serial monitor.
// *** note: debug messages go out over Serial and will be transmitted to these if set to Serial and the unit is connected to the USB host of another unit ***
#define beaconSerial  Serial

/*

Memory Usage on Teensy 4.1:

RADIO_ROLE = 0

Serial
  FLASH: code:276196, data:92720, headers:8932   free for files:7748616
   RAM1: variables:127744, code:223000, padding:6376   free for local variables:167168
   RAM2: variables:332736  free for malloc/new:191552
 EXTRAM: variables:480320

*/
