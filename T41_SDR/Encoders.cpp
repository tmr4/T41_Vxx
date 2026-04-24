
#include "SDT.h"

#include "Button.h"
#include "ButtonProc.h"
#include "CWProcessing.h"
#include "Display.h"
//#include "EEPROM.h"
#include "Encoders.h"
#include "Filter.h"
#include "ft8.h"
#include "hardware.h"
#include "Menu.h"
#include "MenuProc.h"
#include "Tune.h"
#include "Utility.h"

//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

#define ENCODER_FACTOR            0.4  // gives 100 Hz change with standard Borne encoders

bool resetTuningFlag, getEncoderValueFlag;
long filter_pos_BW, last_filter_pos_BW;
int posFilterEncoder, lastFilterEncoder;

float adjustVolEncoder = 0.0;

volatile int tuneChange;
volatile int menuEncoderMove;
volatile long fineTuneEncoderMove;

//-------------------------------------------------------------------------------------------------------------
// Code
//-------------------------------------------------------------------------------------------------------------

void ProcessFilterEncoder() {
  int filterChange = (posFilterEncoder - lastFilterEncoder) * 50 * ENCODER_FACTOR;

  if(filterChange == 0) return;

  lastFilterEncoder = posFilterEncoder;

  SetBWFilters(filterChange);
}

void ProcessMenuEncoder() {
  if((calibrateItem >= 1) && (calibrateItem <= 3)) return;

  // interpret encoder according to flag settings
  if(getEncoderValueFlag || (displayState == DISPLAY_FULL_MENU)) {
    return; // menuEncoderMove processed in GetEncoderValueLive and GetMenuValueLoop routines
  }

  if(liveNoiseFloorFlag == 2) {
    // we're setting noise floor
    currentNoiseFloor[t41.ActiveBand] += menuEncoderMove;
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

/*****
  Purpose: set center tune frequency based on changes to tuneChange
*****/
bool ProcessCenterTuneEncoder(bool readEncoder /* = false */) {
  // read encoder if requested
  // *** some hardware versions have interrupt driven tuning encoder
  // eliminating the need to read it here ***
  if(readEncoder) tuneChange = ReadTuneEncoder();

  if(tuneChange == 0)
    return false;

  if(t41.RadioMode == CW_MODE && decoderFlag == ON) {
    ResetHistograms();
  }

  // *** TODO: from v12, validate v11 calibration routines
  // center tune used in calibration routines, return to process
  //   - receive calibrate adjusts noise floor
  //   - transmit calibrate adjusts image value
  //   - two tone adjusts tone 1
  if((calibrateItem >= 1) && (calibrateItem <= 3)) return false; // *** TODO: validate required calibration return value ***

  SetCenterTune((long)freqIncrement * tuneChange);

  tuneChange = 0;
  return true;
}
