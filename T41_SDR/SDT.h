#pragma once

#include "T41Config.h"

#include <Arduino.h>

#ifndef float32_t
typedef float float32_t;
#endif

#ifndef uint8_t
typedef __uint8_t uint8_t;
#endif

#include "src\hardwareConfig.h"
#include "src\hardware.h"

//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

// Radio State
#define SSB_RECEIVE_STATE 0
#define SSB_TRANSMIT_STATE 1
#define CW_RECEIVE_STATE 2
#define CW_TRANSMIT_STRAIGHT_STATE 3
#define CW_TRANSMIT_KEYER_STATE 4
#define DATA_RECEIVE_STATE 5
#define DATA_TRANSMIT_STATE 6
#define CALIBRATE_RECEIVE_STATE 7
#define CALIBRATE_TRANSMIT_STATE 8
#define CALIBRATE_TWOTONE_STATE 9
#define CALIBRATE_DONE_STATE 10

// SSB/CW demodulation modes
#define DEMOD_MIN                   0
#define DEMOD_USB                   0
#define DEMOD_LSB                   1
#define DEMOD_AM                    2
#define DEMOD_SAM                   3
#define DEMOD_NFM                   4
#define DEMOD_MAX                   4

// Data demodulation modes
#define DEMOD_DATA_MIN              (DEMOD_MAX + 1)
#define DEMOD_FT8                   (DEMOD_DATA_MIN + 0) // assumes a WSJT-X hook up
#define DEMOD_FT8_DECODE            (DEMOD_DATA_MIN + 1) // demodulate FT8 signals via antenna input as USB for audio
#define DEMOD_FT8_WAV               (DEMOD_DATA_MIN + 2)
#define DEMOD_PSK31                 (DEMOD_DATA_MIN + 3)
#define DEMOD_PSK31_WAV             (DEMOD_DATA_MIN + 4)
#define DEMOD_DATA_MAX              (DEMOD_DATA_MIN + 2) // skip psk31 for now

#define NUMBER_OF_BANDS           7
#define BAND_80M                  0
#define BAND_40M                  1
#define BAND_20M                  2
#define BAND_17M                  3
#define BAND_15M                  4
#define BAND_12M                  5
#define BAND_10M                  6

#define SSB_MODE                  0
#define CW_MODE                   1
#define DATA_MODE                 2

#define OFF                       0
#define ON                        1

//---- Global Teensy 4.1 Pin assignments
#define RXTX                        22    // Transmit/Receive
#define KEYER_DAH_INPUT_RING        35    // Ring connection for keyer  -- default for righthanded user
#define KEYER_DIT_INPUT_TIP         36    // Tip connection for keyer

// Pins 0 and 1 are usually reserved for the USB COM port communications
// On the Teensy 4.1 board, pins GND, 0-12, and pins 13-23, 3.3V, GND, and
// Vin are "covered up" by the Audio board. However, not all of those pins are
// actually used by the board. See: https://www.pjrc.com/store/teensy3_audio.html
// Filter Board pins
#define FILTERPIN80M 30    // 80M filter relay
#define FILTERPIN40M 31    // 40M filter relay
#define FILTERPIN20M 28    // 20M filter relay
#define FILTERPIN15M 29    // 15M filter relay

//---- End Global Teensy 4.1 Pin assignments

#define CLEAR_VAR(x) memset(x, 0, sizeof(x))
#define SET_VAR(x,y) memset(x, y, sizeof(x))

// delete once we get rid of global working variables
#include "gwv.h"

// radio hardware and state global variables

extern float sampleRate, intermediateFreq;

extern int radioState, lastState;  // Used by the loop to monitor current state.

extern int volSetting;

extern float32_t audioBufferL[];
extern float32_t audioBufferR[];
extern float32_t audioBufferL_EX[];
extern float32_t audioBufferR_EX[];
extern float32_t audioBufferTemp[];

typedef struct {
  long freq;      // Current frequency in Hz
  long fBandLow;  // Lower band edge
  long fBandHigh; // Upper band edge
  const char* name; // name of band
  int demod;
  int fLoCut;
  int fHiCut;
  int rfGain;
  long calFreq; // receive IQ calibration frequency
  float32_t gainCorrection; // is hardware dependent and has to be calibrated ONCE and hardcoded in the band table
  int agcThresh;
  int16_t pixelOffset;
} band;

extern band bands[];

extern int bandswitchPins[];
