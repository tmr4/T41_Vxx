#pragma once

#include "T41Config.h"

#include <Arduino.h>

#include "t41Property.h"

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

// radio modes                      // associated demod modes
#define SSB_MODE                  0 // USB, LSB
#define CW_MODE                   1 // USB, LSB
#define DSB_MODE                  2 // AM, SAM, FM (narrow band FM)
#define DATA_MODE                 3 // FT8 (external), FT8 (internal), FT8 (wav for testing)

// demodulation modes
// SSB/CW
#define DEMOD_USB                   0
#define DEMOD_LSB                   1
// DSB
#define DEMOD_AM                    2
#define DEMOD_SAM                   3
#define DEMOD_NFM                   4
// Data
#define DEMOD_FT8                   5
#define DEMOD_FT8_INTERNAL          6
#define DEMOD_FT8_WAV               7
#define DEMOD_PSK31                 8
#define DEMOD_PSK31_WAV             9

#define NUMBER_OF_BANDS           7
#define BAND_80M                  0
#define BAND_40M                  1
#define BAND_20M                  2
#define BAND_17M                  3
#define BAND_15M                  4
#define BAND_12M                  5
#define BAND_10M                  6

// radio status
#define IB_ITEM_VOL       0
#define IB_ITEM_AGC       1
#define IB_ITEM_TUNE      2
#define IB_ITEM_FINE      3
#define IB_ITEM_ZOOM      4
#define IB_ITEM_FLOOR     5
#define IB_ITEM_NOTCH     6
#define IB_ITEM_COMPRESS  7
#define IB_ITEM_FILTER    8
#define IB_ITEM_RFGAIN    9
#define IB_ITEM_EQUALIZER 10
#define IB_ITEM_DECODER   11
#define IB_ITEM_KEY       12
#define IB_ITEM_KEYER     13
#define IB_ITEM_FT8       14
#define IB_ITEM_FT8_INT   15
#define IB_ITEM_FT8_TX    16
#define IB_ITEM_FT8_CQ    17
#define IB_ITEM_FT8_TXF   18
#define IB_ITEM_FT8_RXF   19
#define IB_ITEM_STACK     20
#define IB_ITEM_HEAP      21
#define IB_ITEM_TEMP      22
#define IB_ITEM_LOAD      23

// remote modes
#define REMOTE_NOT_AVAIL      0
#define REMOTE_WAITING        1
#define REMOTE_CONNECTED      2
#define REMOTE_LOST           3

#define OFF                       0
#define ON                        1

#define VFO_A                 0
#define VFO_B                 1
#define VFO_SPLIT             2

#define CLEAR_VAR(x) memset(x, 0, sizeof(x))
#define SET_VAR(x,y) memset(x, y, sizeof(x))

// delete once we get rid of global working variables
#include "gwv.h"

// radio hardware and state global variables

extern float sampleRate, intermediateFreq;

extern int radioState, lastState;  // used by the main loop to monitor current state
extern int currentDemodMode;

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

// *** TODO: move to appropriate front panel hardware ***
extern int bandswitchPins[];

// radio status
// *** TODO: some display specific, needs generalized ***
extern bool beaconFlag;
extern bool infoBoxItemActive[];

//-------------------------------------------------------------------------------------------------------------
// Code
//-------------------------------------------------------------------------------------------------------------

// radio status update
// *** TODO: display specific, needs generalized ***
void UpdateInfoBox();
void UpdateInfoBoxItem(uint8_t item);
void UpdateClock();
void UpdateDecodeLockIndicator();
void UpdateIBWPM();
void ClearInfoBoxKeyer();
void SetFtActive(int flag);
void HighlightIBItem(uint8_t item, int color);
void MouseButtonInfoBox(int button, int cursorX, int cursorY);
void MouseWheelInfoBox(int wheel, int x, int y);

void BeaconInit();
void BeaconExit();
void BeaconLoop();

void ButtonBearing();
void BearingMaps();
