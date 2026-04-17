#pragma once

// vAP Audio Platform specific hardware header file

#include "hardwareConfig.h"

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

// Audio Platform free Teensy w/ Audio board pins:
// Notes:
// Audio Platform comes with hardwired display, encoders, midi, flash
// (Teensy sides w/ Audio Platform upright)
// These are the Audio Platform GPIO pins
// Left side: 14-17,22,40,41
// Right side: 10,28,29,32

// free Audio Platform pins after assignments below:
// *** unused T41 inputs assigned to pin 28 ***
// *** unused T41 outputs assigned to pin 22 ***
// Left side: (22)
// Right side: (28)

// *** Input Pins ***
// Audio Platform encoders 1-4, left to right: volume, filter, fine tune, center tune
#define VOLUME_ENCODER_A         3
#define VOLUME_ENCODER_B         4
#define VOLUME_SWITCH           24
#define FILTER_ENCODER_A        25  // these are wired in reverse on PS
#define FILTER_ENCODER_B        30
#define FILTER_SWITCH           31
#define FINETUNE_ENCODER_A      33  // these are wired in reverse on PS
#define FINETUNE_ENCODER_B      34
#define FINETUNE_SWITCH         35
#define TUNE_ENCODER_A          36
#define TUNE_ENCODER_B          37
#define TUNE_SWITCH             38

#define PTT                     28    // TX input
#define KEYER_DAH_INPUT_RING    28    // Ring connection for keyer  -- default for righthanded user
#define KEYER_DIT_INPUT_TIP     28    // Tip connection for keyer
#define BUSY_ANALOG_PIN         17    // must be different than other inputs *** TODO: why? ***

// *** Output Pins ***

#define RXTX         22    // TX/RX relay
#define MUTE         22    // Mute Audio,  HIGH = "On" Audio available from Audio PA, LOW = Mute audio

// the Audio Platform uses an RA8875 display
#define TFT_DC                  9
#define TFT_CS                  5
#define TFT_MOSI                26
#define TFT_MISO                39
#define TFT_SCLK                27
#define TFT_RST                 255

// Filter Board pins
#define FILTERPIN80M            22    // 80M filter relay
#define FILTERPIN40M            22    // 40M filter relay
#define FILTERPIN20M            22    // 20M filter relay
#define FILTERPIN15M            22    // 15M filter relay

// GPIO pins: even top, odd bottom, increasing right to left (normal Audio Platform orientation)
#define PROFILER_MAINLOOP_PIN         10 // GPIO #1
#define PROFILER_PROCESS_PIN          41 // GPIO #2
#define PROFILER_DRAWFREQSPEC_PIN     14 // GPIO #3
#define PROFILER_DRAWAUDIOSPEC_PIN    40 // GPIO #4
#define PROFILER_FT8PROCESSBLOCK_PIN  15 // GPIO #5
#define PROFILER_FT8GETDATA_PIN       32 // GPIO #6
#define PROFILER_FT8DECODE_PIN        16 // GPIO #7
#define PROFILER_FT8_TX_PIN           29 // GPIO #8

//---- end of Teensy 4.1 Pin assignments

#include <Rotary.h>                    // https://github.com/brianlow/Rotary

extern Rotary volumeEncoder;
extern Rotary menuChangeEncoder;
extern Rotary fineTuneEncoder;

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
