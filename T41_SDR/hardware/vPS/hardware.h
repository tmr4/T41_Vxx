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
#define FREQSPEC_OFFSET_10DB  132.0

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
#ifdef PROJECTSYSTEM_VOLUME_ENCODER
#define VOLUME_ENCODER_A         3
#define VOLUME_ENCODER_B         1
#define VOLUME_SWITCH            0
#endif
#ifdef PROJECTSYSTEM_FILTER_ENCODER
#define FILTER_ENCODER_A        11
#define FILTER_ENCODER_B         4
#define FILTER_SWITCH           10
#endif
#ifdef PROJECTSYSTEM_FINETUNE_ENCODER
#define FINETUNE_ENCODER_A      12
#define FINETUNE_ENCODER_B      24
#define FINETUNE_SWITCH         25
#endif
#ifdef PROJECTSYSTEM_TUNE_ENCODER
#define TUNE_ENCODER_A          30
#define TUNE_ENCODER_B          29
#define TUNE_SWITCH             31
#endif

#define PTT                     37    // TX input
#define KEYER_DAH_INPUT_RING    13    // Ring connection for keyer  -- default for righthanded user
#define KEYER_DIT_INPUT_TIP     13    // Tip connection for keyer
#define BUSY_ANALOG_PIN         40    // pin 39 is TFT_MISO on Project System (the pin assigned here is only meaningful when testing switch matrix on non-front panel systems)

// *** conflicts here! ***
// *** these need reassigned when using the MCP expander ***
#ifdef PROJECTSYSTEM_EXPANDED_IO_40
#define INT_PIN_2 40
#endif
#ifdef PROJECTSYSTEM_EXPANDED_IO_41
#define INT_PIN_1 41
#endif

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
#define PROFILER_PROCESS_RX       34
#define PROFILER_DRAW     41
#define PROFILER_ENTRY    14
#define PROFILER_PROCESS_FRAME      35
#define PROFILER_RX_TX       36
#define PROFILER_DECODE_FT8       38
#define PROFILER_OTHER       16

// other

#ifdef PROJECTSYSTEM_ENCODER_1
#define ENCODER_1_SWITCH
#endif
#ifdef PROJECTSYSTEM_ENCODER_2
#define ENCODER_2_SWITCH
#endif
#ifdef PROJECTSYSTEM_ENCODER_3
#define ENCODER_3_SWITCH
#endif
#ifdef PROJECTSYSTEM_ENCODER_4
#define ENCODER_4_SWITCH
#endif


//---- end of Teensy 4.1 Pin assignments

#ifdef PROJECTSYSTEM_VOLUME_ENCODER
#include <Rotary.h>                    // https://github.com/brianlow/Rotary

extern Rotary volumeEncoder;
#endif
#ifdef PROJECTSYSTEM_FILTER_ENCODER
#include <Rotary.h>                    // https://github.com/brianlow/Rotary

extern Rotary menuChangeEncoder;
#endif
#ifdef PROJECTSYSTEM_FINETUNE_ENCODER
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

#ifdef PROJECTSYSTEM_VOLUME_ENCODER
void EncodersInit();
void EncoderVolumeISR();
#endif

#ifdef PROJECTSYSTEM_FILTER_ENCODER
void EncodersInit();
void EncoderMenuChangeFilterISR();
#endif

#ifdef PROJECTSYSTEM_FINETUNE_ENCODER
void EncodersInit();
void EncoderFineTuneISR();
#endif

#ifdef PROJECTSYSTEM_TUNE_ENCODER
void EncodersInit();
void EncoderCenterTuneISR();
#endif

#ifdef PROJECTSYSTEM_ENCODER_MCP
#endif

void InitFrontPanel();

#ifdef FRONT_PANEL_POLLING_OPS
inline void PollFrontPanel() {
  if(digitalRead(INT_PIN_1) == LOW) {
    Mcp1Isr();
  }
  if(digitalRead(INT_PIN_2) == LOW) {
    Mcp2Isr();
  }
}
#else
inline void PollFrontPanel() {}
#endif

//------------
// Calibrate.h
inline float GetEncoderValueLive(float minValue, float maxValue, float startValue, float increment, char prompt[]) { return 0.0; }
