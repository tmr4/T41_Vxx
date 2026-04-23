#pragma once

// minimum hardware header file

/*

Minimum hardware assumes a T41 main board or equivalent so that RX signal can be processed for testing

*/

// *** TODO: work up min defines needed to compile w/o hardware ***
//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

//------------
// Display.h

// radio specific display calibration factors
#define FREQSPEC_OFFSET_10DB  80

//------------
// Encoders.h

//---- Teensy 4.1 Pin assignments
// All Teensy pin assignments are made here.  This makes it easier
// when working up pin assignments for new hardware. The pins are
// divided into input and output to facilitate assignment on
// development boards for testing where some pins may be assigned
// for other purposes and some hardware may not be present.
// Conflicts with normal T41 pin assignments can cause operational
// problems.

// Project System free Teensy w/ Audio board pins:
// Notes:
//  pin 2 is touchscreen interrupt (this seems hardwired, thus a touch pulls this pin low))
//  pins 6,10-13 are associated with Audio board SD card and memory chip and aren't available if these are used
// (Teensy sides as Project System display is to right)
// Left side: 13-17,22,33-38,40,41
// Right side: 0,1,3,4,10-12,24,25,28-31

// free project system pins after assignments below:
// *** unused T41 inputs assigned to pin 13 ***
// *** unused T41 outputs assigned to pin 15 ***
// Left side: (13),(15),17
// Right side: 28

// *** Input Pins ***

#define PTT                     37    // TX input
#define KEYER_DAH_INPUT_RING    13    // Ring connection for keyer  -- default for righthanded user
#define KEYER_DIT_INPUT_TIP     13    // Tip connection for keyer
#define BUSY_ANALOG_PIN         40    // pin 39 is TFT_MISO on Project System (the pin assigned here is only meaningful when testing switch matrix on non-front panel systems)

// *** Output Pins ***

#define RXTX         22    // TX/RX relay
#define MUTE         15    // Mute Audio,  HIGH = "On" Audio available from Audio PA, LOW = Mute audio

// the Project System uses an RA8875 display
#define TFT_DC                  9
#define TFT_CS                  5
#define TFT_MOSI                26
#define TFT_MISO                39
#define TFT_SCLK                27
#define TFT_RST                 255

// Filter Board pins
#define FILTERPIN80M            15    // 80M filter relay
#define FILTERPIN40M            15    // 40M filter relay
#define FILTERPIN20M            15    // 20M filter relay
#define FILTERPIN15M            15    // 15M filter relay

#define PROFILER_MAINLOOP         33
#define PROFILER_PROCESS_RX          34
#define PROFILER_DRAWFREQSPEC     41
#define PROFILER_DRAWAUDIOSPEC    14
#define PROFILER_PROCESS_FT8  35
#define PROFILER_FT8_CAT_RX       36
#define PROFILER_DECODE_FT8        38
#define PROFILER_FT8_CAT_TX           16


//---- end of Teensy 4.1 Pin assignments

//------------
// Menu.cpp

#define MENU_RF_OPTIONS  "Power level", "Gain", "Cancel"
//#define MENU_CAL_OPTIONS "Freq Cal", "CW PA Cal", "Rec Cal", "Xmit Cal", "SSB PA Cal", "Cancel"
#define MENU_CAL_OPTIONS "Frequency", "CW Pwr", "SSB PA", "IQ", "Two Tone", "Cancel"
#define MENU_RF_COUNT    3
#define MENU_CAL_COUNT   6

//------------
// Process.cpp

// radio specific display calibration factors
#define AUDIO_SPEC_SHIFT     105.0
#define AUDIO_SPEC_SHIFT_NFM 105.0
#define VOL_FACTOR            20.0
#define AUDIO_SCALER_NFM       0.005

//------------
// T41_SDR.ino

// hardware/band specific signal strength adjustment
// these were set with AD3 signal generator at S9 (1mW signal attenuated -73dB)
// TODO: signal strength varies by demod mode as noted; consider refinement ***
#define GAIN_CORRECTION_80M -4.0
#define GAIN_CORRECTION_40M  1.0  // gives -71 on USB
#define GAIN_CORRECTION_20M -3.0
#define GAIN_CORRECTION_17M -3.0
#define GAIN_CORRECTION_15M -1.0
#define GAIN_CORRECTION_12M -1.0
#define GAIN_CORRECTION_10M -1.0

//-------------------------------------------------------------------------------------------------------------
// Code
//-------------------------------------------------------------------------------------------------------------

inline void PollFrontPanel() {}
