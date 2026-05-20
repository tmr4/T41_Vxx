// vAP Audio Platform specific hardware config file for running on Audio Platform

#pragma once

#define VERSION "vAP_dev/v0.02" // up to 14 characters can fit *** can adjust infobox ShowVersion routine to accommodate more if needed ***
#define RADIO_ID 02             // 0-999, radio ID is reported as 24 to WSJT-X (Kenwood TS-890S)

#define PROFILER_ACTIVE

#define MASTER_CLK_MULT 4ULL // FOURSQRP QSD frontend requires 4x clock

// *** needs to be true if using center tune encoder on MCP23017
#define READ_CENTERTUNE_ENCODER false // set to false if interrupt driven

// pick one of the following display configurations
// *** the AP display (DISPLAY_LANDSCAPE) is offset up 2 pixels compared to PS display (DISPLAY_FLIPPED)
//     the bottom of the display however isn't offset by this amount, thus if image is shifted down by 2
//     pixels, image is written off screen. I've chosen to retain PS offsets for now. ***
// *** TODO: examine this effect on 5" displays and other 7" displays to verify consistent operation ***
// *** the 7" display on the PS doesn't exhibit the same offset w/ DISPLAY_LANDSCAPE, thus the AP display is perhaps defective ***
#define DISPLAY_LANDSCAPE
//#define DISPLAY_FLIPPED

#define VOLUME_REVERSED false
#define FILTER_REVERSED false
#define MAIN_TUNE_REVERSED false
#define FINE_TUNE_REVERSED false

// set to true for mouse/keyboard support
// uses about 44k of stack
#define HOST_KEYBOARD_MOUSE_SUPPORT true

// Remote control
/*
  T41_Vxx supports remote operations between a T41 and remote device or WSJT-X. To enable this
  feature set DEVICE_REMOTE_OPS_MODE to the role of this unit, where 0 disables remote operation,
  1 indicatesthis is a T41 connected to a PC running WSJT-X, 2 inicates this is a remote device
  communicating with a T41, and 3 inidicates this is a T41 communicating with a remote device.
  The device is then configured as follows:

  T41 connected to a PC running WSJT-X (DEVICE_REMOTE_OPS_MODE=1)
    - USB cable from the USB serial connection on this unit (T41) to a USB connection on the PC
    - Compile with an Audio option selected, such as "Serial + MIDI + Audio"

  Remote device connected to a T41 (DEVICE_REMOTE_OPS_MODE=2)
    - USB cable from the USB serial connection on this device (remote) to the USB host connection on the T41
    - Compile with at least "Dual Serial" selected

  T41 connected to a remote devise (DEVICE_REMOTE_OPS_MODE=1)
    - USB cable from the USB host connection on this unit (T41) to the USB serial connection on the remote

  For operation with a remote device, DEVICE_REMOTE_OPS_MODE equals 2 or 3, select the type of operation,
  CAT control and/or Audio transfer, by setting REMOTE_CAT_CONTROL and REMOTE_AUDIO_DATA appropriately.
*/

// set the remote operation role of this device
#define DEVICE_REMOTE_OPS_MODE  4 // 0=none, 1=WSJT-X, Remote over USB (2=remote, 3=T41), Remote over Ethernet (4=remote, 5=T41)

// set the desired remote services below to true to enable remote CAT control/audio
// *** IQ audio is sent from T41 device USB Host connector to the remote USB (Serial) connector ***
#define REMOTE_CAT_CONTROL      true
#define REMOTE_AUDIO_DATA       true

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
#define controlSerial Serial
#define controlAudio  Serial
#define beaconSerial  Serial
#define wsjtSerial    Serial

// the following are disabled by defualt and will be set automatically depending on the remote mode selected
#define T41_WSJT_CAT_AUDIO        false
#define CAT_CONTROL_T41           false
#define CAT_CONTROL_REMOTE        false
#define CAT_CONTROL_T41_USB_HOST  false
#define REC_IQ_FROM_T41_USB       false
#define SEND_IQ_TO_REMOTE_USB     false

// automatically configure radio for selected remote operation and services
#if (DEVICE_REMOTE_OPS_MODE < 0) || (DEVICE_REMOTE_OPS_MODE > 5)
  #undef DEVICE_REMOTE_OPS_MODE
  #define DEVICE_REMOTE_OPS_MODE 0
#else
  #if DEVICE_REMOTE_OPS_MODE == 1
    // *** for passing CAT/audio back and forth with WSJT-X over USB at 44.1kHz sample rate in FT8 mode ***
    // USB cable from the USB serial connection on this unit (T41) to a USB connection on the PC
    // Compile with an Audio option selected, such as "Serial + MIDI + Audio"
    #undef T41_WSJT_CAT_AUDIO
    #define T41_WSJT_CAT_AUDIO true
    #undef wsjtSerial
    #define wsjtSerial SerialUSB1
  #elif DEVICE_REMOTE_OPS_MODE == 2
    // remote
    #if REMOTE_CAT_CONTROL || REMOTE_AUDIO_DATA
      #undef CAT_CONTROL_REMOTE
      #define CAT_CONTROL_REMOTE true
    #endif
    #if REMOTE_CAT_CONTROL
      #undef controlSerial
      #define controlSerial Serial
    #endif
    #if REMOTE_AUDIO_DATA
      #undef REC_IQ_FROM_T41_USB
      #define REC_IQ_FROM_T41_USB true
      #undef controlAudio
      #define controlAudio SerialUSB1
    #endif
  #elif DEVICE_REMOTE_OPS_MODE == 3
    // T41
    #if REMOTE_CAT_CONTROL || REMOTE_AUDIO_DATA
      #undef CAT_CONTROL_T41_USB_HOST
      #define CAT_CONTROL_T41_USB_HOST true
    #endif
    #if REMOTE_CAT_CONTROL
      #undef controlSerial
      #define controlSerial usbHostSerial
    #endif
    #if REMOTE_AUDIO_DATA
      #undef SEND_IQ_TO_REMOTE_USB
      #define SEND_IQ_TO_REMOTE_USB true
      #undef controlAudio
      #define controlAudio usbHostSerial1
    #endif
  #elif DEVICE_REMOTE_OPS_MODE == 4
    // remote Ethernet
    #if REMOTE_CAT_CONTROL || REMOTE_AUDIO_DATA
      #undef CAT_CONTROL_REMOTE
      #define CAT_CONTROL_REMOTE true
    #endif
    #if REMOTE_CAT_CONTROL
      #undef controlSerial
      #define controlSerial ethernetControl
    #endif
    #if REMOTE_AUDIO_DATA
      #undef REC_IQ_FROM_T41_ETHER
      #define REC_IQ_FROM_T41_ETHER true
      // *** TODO: can this be made generic for USB and Ethernet? ***
      //#undef controlAudio
      //#define controlAudio SerialUSB1
    #endif
  #elif DEVICE_REMOTE_OPS_MODE == 5
    // T41 Ethernet
    #if REMOTE_CAT_CONTROL || REMOTE_AUDIO_DATA
      #undef CAT_CONTROL_T41
      #define CAT_CONTROL_T41 true
    #endif
    #if REMOTE_CAT_CONTROL
      #undef controlSerial
      #define controlSerial ethernetControl
    #endif
    #if REMOTE_AUDIO_DATA
      #undef SEND_IQ_TO_REMOTE_ETHER
      #define SEND_IQ_TO_REMOTE_ETHER true
      // *** TODO: can this be made generic for USB and Ethernet? ***
      //#undef controlAudio
      //#define controlAudio usbHostSerial1
    #endif
  #endif
#endif
