#pragma once

// vMP Mini Platform specific hardware header file

#include "hardwareConfig.h"

//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

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

// Mini Platform free Teensy w/ Audio board pins:
// Notes:
// Mini Platform comes with hardwired display (*** update ***), encoders, midi, flash
// (Teensy sides w/ Mini Platform upright)
// These are the Mini Platform GPIO pins
// Left side:
// Right side: 24,25

// free Mini Platform pins after assignments below:
// *** unused T41 inputs assigned to pin 17 ***
// *** unused T41 outputs assigned to pin 0 ***
// Left side: (17),()
// Right side: (0),(1)

// *** Input Pins ***
#define VOLUME_ENCODER_A         40
#define VOLUME_ENCODER_B         40
#define VOLUME_SWITCH            40
#define FILTER_ENCODER_A         40
#define FILTER_ENCODER_B         40
#define FILTER_SWITCH            40
#define FINETUNE_ENCODER_A       40
#define FINETUNE_ENCODER_B       40
#define FINETUNE_SWITCH          40
#define TUNE_ENCODER_A           40
#define TUNE_ENCODER_B           40
#define TUNE_SWITCH              40

#define PTT                      40    // TX input
#define KEYER_DAH_INPUT_RING     40    // Ring connection for keyer  -- default for righthanded user
#define KEYER_DIT_INPUT_TIP      40    // Tip connection for keyer
#define BUSY_ANALOG_PIN          41    // must be different than other inputs *** TODO: why? ***

// *** Output Pins ***

#define RXTX        38    // TX/RX relay
#define MUTE        38    // Mute Audio,  HIGH = "On" Audio available from Audio PA, LOW = Mute audio

// the Audio Platform uses an RA8875 display
#define TFT_DC                  9
#define TFT_CS                  10
#define TFT_MOSI                11
#define TFT_MISO                12
#define TFT_SCLK                13
#define TFT_RST                 255

// Filter Board pins
#define FILTERPIN80M            38    // 80M filter relay
#define FILTERPIN40M            38    // 40M filter relay
#define FILTERPIN20M            38    // 20M filter relay
#define FILTERPIN15M            38    // 15M filter relay

// GPIO connector pins bottom to top as shown on breadboard adapter
#define PROFILER_MAINLOOP          0
#define PROFILER_PROCESS_RX           1
#define PROFILER_DRAW      2
#define PROFILER_ENTRY     3
#define PROFILER_PROCESS_FRAME  14
#define PROFILER_RX_TX       15
#define PROFILER_DECODE_FT8        16
#define PROFILER_OTHER           17

//---- end of Teensy 4.1 Pin assignments

#include <Rotary.h>                    // https://github.com/brianlow/Rotary

extern Rotary volumeEncoder;
extern Rotary menuChangeEncoder;
extern Rotary fineTuneEncoder;

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
// Process.h

void RemoveDCBias();
float CalcSignalStrength();

//------------
// T41_SDR.ino

void InitHardware();
void SoftResetHardware();
void ConfigRadioStateHardware();
void HardwareLoopStart();

//------------
// Encoders.h

void EncodersInit();
void EncoderVolumeISR();
void EncoderMenuChangeFilterISR();
void EncoderFineTuneISR();
void EncoderCenterTuneISR();

inline void PollFrontPanel() {}
