// vPS specific hardware config file for running on Project System

#pragma once

#define VERSION "vPS_dev/v0.03" // up to 14 characters can fit *** can adjust infobox ShowVersion routine to accommodate more if needed ***
#define RADIO_ID 01             // 0-999, radio ID is reported as 24 to WSJT-X (Kenwood TS-890S)
#define RADIO_ROLE 7            // 0:standard ops, 1:WSJT-X, *** See Remote operation below for additional settings ***

#define PROFILER_ACTIVE
#define CAT_SPY false           // set to true to examine CAT communication traffic in waterfall area

#define MASTER_CLK_MULT 4ULL // FOURSQRP QSD frontend requires 4x clock

// pick one of the following display configurations
//#define DISPLAY_LANDSCAPE
#define DISPLAY_FLIPPED

// *** needs to be true if using center tune encoder on MCP23017
#define READ_CENTERTUNE_ENCODER false // set to false if interrupt driven

#define USE_BUFFERED_FT8_WAV // buffered wav file used for internal FT8 testing

#define PROJECTSYSTEM_VOLUME_ENCODER
#define PROJECTSYSTEM_FILTER_ENCODER
#define PROJECTSYSTEM_FINETUNE_ENCODER
#define PROJECTSYSTEM_TUNE_ENCODER
#define PROJECTSYSTEM_SWITCH_MATRIX

// other defines for testing v11/v12 type encoders w/ project system
// *** currently Rotary and Rotary_V12 can't coexist so only ENCODER_1 or ENCODER_MCP be enabled ***
//#define PROJECTSYSTEM_ENCODER_1
//#define PROJECTSYSTEM_ENCODER_2
//#define PROJECTSYSTEM_ENCODER_3
//#define PROJECTSYSTEM_ENCODER_4
//#define PROJECTSYSTEM_ENCODER_MCP

#define VOLUME_REVERSED false
#define FILTER_REVERSED false
#define MAIN_TUNE_REVERSED false
#define FINE_TUNE_REVERSED false

//#define FRONT_PANEL_POLLING_OPS

// can also use Project System I/O Expanders
//#define PROJECTSYSTEM_EXPANDED_IO_40
//#define PROJECTSYSTEM_EXPANDED_IO_41

#ifdef PROJECTSYSTEM_EXPANDED_IO_40
#define PROJECTSYSTEM_MCP23017_ADDR 0x24
// the Project System only has one MCP23017
// can test other half of front panel by selecting address 0x24 (or 0x20) below
// (consider coding for the sn74cbtlv3251 if fully functional front panel on Project System is needed)
#define V12_PANEL_MCP23017_ADDR_1 0x24

// can also solder JP1 for address 0x20
//#define V12_PANEL_MCP23017_ADDR_1 0x20
#endif

// set to true for mouse/keyboard support
// uses about 44k of stack
#define HOST_KEYBOARD_MOUSE_SUPPORT false

/*
  Remote operation:

  T41_Vxx supports operations between a T41 and a remote device, such as WSJT-X, other PC app, or
  other T41_Vxx enabled devices. This section describes how to configure the T41 and remote device
  for such operation and specifies the compiler options required for each. The limitations of such
  operation are also discussed. The type of connection desired between the T41 and remote device
  may require compiling the software with certain compiler options and these might preclude certain
  T41 capabilities.  For example, WSJT-X w/ USB Audio/CAT control requires the "Serial + MIDI + Audio"
  USB Type compile option. There is only one USB serial object available with USB audio enabled.  This
  Serial object is reserved for WSJT-X use.  Any other use could disrupt WSJT-X control of the T41.

  The following remote operating modes are available:
    Mode                        T41 RADIO_ROLE      Remote RADIO_ROLE
    None                          0                     na
    WSJT-X                        1                     na
    Remote (USB)                  3                     2
    Remote (Ethernet)             5                     4
    Remote (USB or Ethernet)      7                     6
    Auto Cal (USB)                9                     8
    CAT only (Ethernet)          11                    10
    CAT only (USB)               13                    12

  The following minimum USB Type compile option is required for each operating modes:
    Mode                           T41                      Remote
    None                          Serial                      na
    WSJT-X                        Serial + MIDI + Audio       *
    Remote (USB)**                Serial                     Dual Serial
    Remote (Ethernet)**           Serial                     Serial
    Remote (USB or Ethernet)***   Serial                     Dual Serial
    Auto Cal (USB)                Serial                     Serial
    CAT only (Ethernet)           Serial                     Serial
    CAT only (USB)                Serial                     Serial

    *   - configure WSJT-X for the serial COM assiciated with the T41 and 44.1kHz audio
    **  - remote operation includes CAT control and high-speed IQ data transfer for spectrum display and audio
    *** - plug and play

  To enable remote operation, set RADIO_ROLE to the role of this device, compile and upload.
  Likewise, set the role of the companion device, compile again and upload to that device. Connect the two
  devices with the selected cable type. Communication between the two devices should commence once the
  connection is established.

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

// the following are disabled by defualt and will be set automatically depending on the remote mode selected
#define T41_WSJT_CAT_AUDIO        false

// automatically configure radio for selected remote operation and services
#if (RADIO_ROLE < 0) || (RADIO_ROLE > 13)
  #undef RADIO_ROLE
  #define RADIO_ROLE 0
#else
  #if RADIO_ROLE == 7
    // *** for passing CAT/audio back and forth with WSJT-X over USB at 44.1kHz sample rate in FT8 mode ***
    // USB cable from the USB serial connection on this unit (T41) to a USB connection on the PC
    // Compile with an Audio option selected, such as "Serial + MIDI + Audio"
    #undef T41_WSJT_CAT_AUDIO
    #define T41_WSJT_CAT_AUDIO true
  #endif
#endif

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

RADIO_ROLE = 7, Serial + Midi + Audio
  FLASH: code:277152, data:93900, headers:8848   free for files:7746564
   RAM1: variables:139072, code:224088, padding:5288   free for local variables:155840
   RAM2: variables:345408  free for malloc/new:178880
 EXTRAM: variables:1200320

*/
