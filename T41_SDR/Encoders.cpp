
#include "SDT.h"

#include "Button.h"
#include "ButtonProc.h"
#include "CWProcessing.h"
#include "Display.h"
//#include "EEPROM.h"
#include "Encoders.h"
#include "Filter.h"
#include "ft8.h"
#include "Menu.h"
#include "MenuProc.h"
#include "Tune.h"
#include "Utility.h"

//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

#define MENU_F_LO_CUT            40

//------------------------- Global Variables ----------
bool volumeChangeFlag, resetTuningFlag, fineTuneFlag, getEncoderValueFlag;
long filter_pos_BW, last_filter_pos_BW;
int posFilterEncoder, lastFilterEncoder;

float adjustVolEncoder = 0.0;

volatile int tuneChange;
volatile int menuEncoderMove;
volatile long fineTuneEncoderMove;

//------------------------- Local Variables ----------


extern int calNFAdjust;

//-------------------------------------------------------------------------------------------------------------
// Code
//-------------------------------------------------------------------------------------------------------------

void AdjustFilterBW(int filterChange) {
  if(lowerAudioFilterActive) { // false - high, true - low filter
    currentFilterLoCut = currentFilterLoCut - filterChange * 50 * ENCODER_FACTOR;

    // restrain filter
    if(currentFilterLoCut < 0.0) currentFilterLoCut = 0.0;
    if(currentFilterLoCut > currentFilterHiCut) currentFilterLoCut = currentFilterHiCut;
  } else {
    currentFilterHiCut = currentFilterHiCut - filterChange * 50 * ENCODER_FACTOR;

    // restrain filter
    if(currentFilterHiCut < currentFilterLoCut) currentFilterHiCut = currentFilterLoCut;
  }
}

/*****
  Purpose: Set bandwidth filters based on accumulated filter encoder changes, update BW values on display

  Parameter list:
    int FW - filter width
*****/
void SetBWFilters() {
  int filterChange = posFilterEncoder - lastFilterEncoder;

  lastFilterEncoder = posFilterEncoder;

  switch(currentDemodMode) {
    case DEMOD_USB:
    case DEMOD_LSB:
    case DEMOD_PSK31:
    case DEMOD_FT8:
    case DEMOD_PSK31_WAV:
    case DEMOD_FT8_INTERNAL:
    case DEMOD_FT8_WAV:
      AdjustFilterBW(filterChange);
      break;

    case DEMOD_AM:
    case DEMOD_SAM:
      currentFilterHiCut = currentFilterHiCut - filterChange * 50 * ENCODER_FACTOR;
      currentFilterLoCut = -currentFilterHiCut;
      break;

    case DEMOD_NFM:
      if(nfmBWFilterActive) {
        filterChange = filter_pos_BW - last_filter_pos_BW;
        last_filter_pos_BW = filter_pos_BW;
        nfmFilterBW = (nfmFilterBW / 2.0 - filterChange * 50 * ENCODER_FACTOR) * 2;
      } else {
        AdjustFilterBW(filterChange);
      }
      break;
  }

  CalcFilters();
}

void ProcessMenuEncoder() {
  if((calibrateItem >= 1) && (calibrateItem <= 3)) return;

  // interpret encoder according to flag settings
  if(getEncoderValueFlag || (displayState == DISPLAY_FULL_MENU)) {
    return; // menuEncoderMove processed in GetEncoderValueLive and GetMenuValueLoop routines
  }

  if(liveNoiseFloorFlag == 2) {
    // we're setting noise floor
    currentNoiseFloor[currentBand] += menuEncoderMove;
  } else {
    if(currentDemodMode == DEMOD_NFM && nfmBWFilterActive) {
      // we're adjusting NFM demod bandwidth
      filter_pos_BW = last_filter_pos_BW - 5 * menuEncoderMove;
    } else {
      // we're adjusting audio spectrum filter
      posFilterEncoder = lastFilterEncoder - 5 * menuEncoderMove;
    }
  }
}
