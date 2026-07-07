#pragma once

// vPS project system specific hardware header file

#include "hardwareConfig.h"

//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

//------------
// Calibrate.h
extern int calNFAdjust;

//------------
// Display.h

// radio specific display calibration factors
#define FREQSPEC_OFFSET_10DB  80.0

//---- Teensy 4.1 Pin assignments
// All Teensy pin assignments are made here.  This makes it easier
// when working up pin assignments for new hardware. The pins are
// divided into input and output to facilitate assignment on
// development boards for testing where some pins may be assigned
// for other purposes and some hardware may not be present.
// Conflicts with normal T41 pin assignments can cause operational
// problems.
// Pins 0 and 1 are usually reserved for the USB COM port communications
// On the Teensy 4.1 board, pins GND, 0-12, and pins 13-23, 3.3V, GND, and
// Vin are "covered up" by the Audio board. However, not all of those pins are
// actually used by the board. See: https://www.pjrc.com/store/teensy3_audio.html

// *** Input Pins ***

#ifdef PROJECTSYSTEM_FINETUNE_ENCODER
#define FINETUNE_ENCODER_A       4
#define FINETUNE_ENCODER_B      24 // pin 5 is TFT_CS on Project System (the pin assigned here is only meaningful when testing fine tune encoder on non-front panel systems)
#endif
#ifdef PROJECTSYSTEM_ENCODER_1
#define VOLUME_ENCODER_A         4
#define VOLUME_ENCODER_B         3
#define ENCODER_1_SWITCH         2
#endif
#ifdef PROJECTSYSTEM_ENCODER_2
#define FILTER_ENCODER_A         29 // switched to reverse direction
#define FILTER_ENCODER_B         28
#define ENCODER_2_SWITCH         30
#endif
#ifdef PROJECTSYSTEM_ENCODER_3
#define FINETUNE_ENCODER_A       28
#define FINETUNE_ENCODER_B       29
#define ENCODER_3_SWITCH         30
#endif

#define PTT                       0 // 37    // TX input
#define KEYER_DAH_INPUT_RING      1 // 35    // Ring connection for keyer  -- default for righthanded user
#define KEYER_DIT_INPUT_TIP       2 // 36    // Tip connection for keyer
#define BUSY_ANALOG_PIN          30 // pin 39 is TFT_MISO on Project System (the pin assigned here is only meaningful when testing switch matrix on non-front panel systems)
                                    // pin 40 is TFT_CS on Prototyping System

// *** Output Pins ***

#define RXTX                     22    // TX/RX relay
#define MUTE                     29 // Mute Audio,  HIGH = "On" Audio available from Audio PA, LOW = Mute audio

// the Prototyping System uses an ILI9341 display
#define BACKLIGHT_PIN               6
#define TFT_DC                      9
#define TFT_CS                      40
#define TFT_MOSI                    11
#define TFT_MISO                    12
#define TFT_SCLK                    13
#define TFT_RST                     255

// Filter Board pins
#define FILTERPIN80M 3 // 30    // 80M filter relay
#define FILTERPIN40M 4 // 31    // 40M filter relay
#define FILTERPIN20M 5 // 28    // 20M filter relay
#define FILTERPIN15M 28 // 29    // 15M filter relay

#define PROFILER_MAINLOOP         33
#define PROFILER_PROCESS_RX          34
#define PROFILER_DRAW     41
#define PROFILER_ENTRY    27
#define PROFILER_PROCESS_FRAME  35
#define PROFILER_RX_TX       36
#define PROFILER_DECODE_FT8        38
#define PROFILER_OTHER           31

//---- end of Teensy 4.1 Pin assignments

#ifdef PROJECTSYSTEM_ENCODER_1
#include <Rotary.h>                    // https://github.com/brianlow/Rotary

extern Rotary volumeEncoder;
#endif
#ifdef PROJECTSYSTEM_ENCODER_2
#include <Rotary.h>                    // https://github.com/brianlow/Rotary

extern Rotary menuChangeEncoder;
#endif
#ifdef PROJECTSYSTEM_ENCODER_3
//#include <Rotary.h>                    // https://github.com/brianlow/Rotary

extern Rotary fineTuneEncoder;
#endif

//------------
// Menu.cpp

#define MENU_RF_OPTIONS  "Power level", "Gain", "Cancel"
#define MENU_RF_COUNT    3

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

//------------
// Encoders.h

#ifdef PROJECTSYSTEM_ENCODER_1
void EncodersInit();
void EncoderVolumeISR();
#endif

#ifdef PROJECTSYSTEM_ENCODER_2
void EncodersInit();
void EncoderMenuChangeFilterISR();
#endif

#ifdef PROJECTSYSTEM_ENCODER_MCP
#endif

//------------
// Calibrate.h
inline float GetEncoderValueLive(float minValue, float maxValue, float startValue, float increment, char prompt[]) { return 0.0; }
