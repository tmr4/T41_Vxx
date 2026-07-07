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
#define RECONFIGURE_STATE           0
#define RECEIVE_STATE               1
#define BEACON_STATE                2
#define SSB_TRANSMIT_STATE          3
#define CW_TRANSMIT_STRAIGHT_STATE  4
#define CW_TRANSMIT_PADDLE_STATE    5
#define CW_TRANSMIT_KEYER_STATE     6
#define DATA_TRANSMIT_STATE         7
// Calibration
#define FREQ_CAL_STATE              8
#define RXIQ_CAL_STATE              9
#define TXIQ_CAL_STATE             10
#define TWOTONE_CAL_STATE          11
#define CWPWR_CAL_STATE            12
#define SSBPWR_CAL_STATE           13

#define CALIBRATE_TRANSMIT_STATE   14
#define CALIBRATE_TWOTONE_STATE    15

// separate receive states not currently needed
//#define SSB_RECEIVE_STATE 0
//#define CW_RECEIVE_STATE 2
//#define DATA_RECEIVE_STATE 5
//#define CALIBRATE_RECEIVE_STATE 7

// radio modes                      // associated demod modes
#define SSB_MODE                  0 // USB, LSB
#define CW_MODE                   1 // USB, LSB
#define DSB_MODE                  2 // AM, SAM, FM (narrow band FM)
#define DATA_MODE                 3 // FT8 (external), FT8 (internal), FT8 (wav for testing)
#define CAL_MODE                  4 // Freq, RX IQ, TX IQ, Two Tone, CW Pwr, SSB Pwr

// demodulation modes
// SSB/CW
#define DEMOD_USB                   0
#define DEMOD_LSB                   1
// DSB
#define DEMOD_AM                    2
#define DEMOD_SAM                   3
#define DEMOD_NFM                   4
// Data
#define DEMOD_FT8                   5 // for use with WSJT_USB_CAT_AUDIO // *** TODO: consider restricting selecting this mode ***
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

// id for various radio elements
// used mainly to identify element for display and remote updates
// T41_ITEM_VOL to T41_ITEM_LOAD are info box items
// listed in comment is supported CAT command prefix for T41 display items and properties
#define T41_ITEMS             37
#define T41_ITEM_VOL           0 // "VO"_cat
#define T41_ITEM_AGC           1 // "GT"_cat
#define T41_ITEM_TUNE          2 // "F0"_cat
#define T41_ITEM_FINE          3 // "F1"_cat
#define T41_ITEM_ZOOM          4 // "ZM"_cat
#define T41_ITEM_FLOOR         5 // "NG"_cat
#define T41_ITEM_NOTCH         6 // "xx"_cat
#define T41_ITEM_FILTER        7 // "N1"_cat
#define T41_ITEM_COMPRESS      8 // "xx"_cat
#define T41_ITEM_RFGAIN        9 // "PG"_cat
#define T41_ITEM_EQUALIZER    10 // "xx"_cat
#define T41_ITEM_DECODER      11 // "xx"_cat
#define T41_ITEM_KEY          12 // "xx"_cat
#define T41_ITEM_KEYER        13 // "xx"_cat
#define T41_ITEM_FT8          14 // "xx"_cat
#define T41_ITEM_FT8_INT      15 // "xx"_cat
#define T41_ITEM_FT8_TX       16 // "xx"_cat
#define T41_ITEM_FT8_CQ       17 // "xx"_cat
#define T41_ITEM_FT8_TXF      18 // "xx"_cat
#define T41_ITEM_FT8_RXF      19 // "xx"_cat
#define T41_ITEM_STACK        20 // "xx"_cat
#define T41_ITEM_HEAP         21 // "xx"_cat
#define T41_ITEM_TEMP         22 // "xx"_cat
#define T41_ITEM_LOAD         23 // "xx"_cat
#define T41_ITEM_MOUSE        24 // "FS"_cat // begin other T41 elements // MouseCenterTuneActive
#define T41_ITEM_NOISE        25 // "NF"_cat // NoiseFloor
#define T41_ITEM_RADIO_MODE   26 // "ME"_cat
#define T41_ITEM_DEMOD_MODE   27 // "MD"_cat
#define T41_ITEM_BAND         28 // "BD"_cat // ActiveBand *** uses BD code to send active band index ***
#define T41_ITEM_POWER        29 // "PC"_cat // TxPower
#define T41_ITEM_FREQ         30 // "FC"_cat // CenterFreq
#define T41_ITEM_NCO          31 // "FF"_cat // NCOFreq
#define T41_ITEM_FHI          32 // "NH"_cat // FilterHiCut
#define T41_ITEM_FLO          33 // "NL"_cat // FilterLoCut
#define T41_ITEM_SCALE        34 // "xx"_cat // FreqSpecScale
#define T41_ITEM_CW_FILTER    35 // "xx"_cat // CWFilterIndex
#define T41_ITEM_CW_DECODER   36 // "xx"_cat
//#define T41_ITEM_   37
//#define T41_ITEM_   38
//#define T41_ITEM_   39

// remote modes
#define REMOTE_NOT_AVAIL      0
#define REMOTE_NOT_CONNECTED  1
#define REMOTE_WAITING        2
#define REMOTE_CONNECTED      3
#define REMOTE_LOST           4

#define OFF                       0
#define ON                        1

#define VFO_A                 0
#define VFO_B                 1
#define VFO_SPLIT             2

#define CLEAR_VAR(x) memset(x, 0, sizeof(x))
#define SET_VAR(x,y) memset(x, y, sizeof(x))

#include "t41Property.h"

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
} band;

extern band bands[];

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
void HighlightIBItem(uint8_t item, int color);
void MouseButtonInfoBox(int button, int cursorX, int cursorY);
void MouseWheelInfoBox(int wheel, int x, int y);

void BeaconInit();
void BeaconExit();
void BeaconLoop();

void ButtonBearing();
void BearingMaps();
