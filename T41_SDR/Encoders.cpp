
#include "SDT.h"

#include "Button.h"
#include "ButtonProc.h"
#include "CWProcessing.h"
#include "Display.h"
//#include "EEPROM.h"
#include "Encoders.h"
#include "Filter.h"
#include "ft8.h"
#include "InfoBox.h"
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
int tuneChange = 0;

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

  switch(bands[currentBand].demod) {
    case DEMOD_USB:
    case DEMOD_LSB:
    case DEMOD_PSK31:
    case DEMOD_FT8:
    case DEMOD_PSK31_WAV:
    case DEMOD_FT8_DECODE:
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

/*****
  Purpose: Use the encoder to change the value of a number in some other function

  Parameter list:
    int minValue                the lowest value allowed
    int maxValue                the largest value allowed
    int startValue              the numeric value to begin the count
    int increment               the amount by which each increment changes the value
    char prompt[]               the input prompt
  Return value:
    int                         the new value
*****/
float GetEncoderValueLive(float minValue, float maxValue, float startValue, float increment, char prompt[]) {
  float currentValue = startValue;

  getEncoderValueFlag = true;

  tft.setFontScale((enum RA8875tsize)1);
  tft.setTextColor(RA8875_WHITE);
  tft.fillRect(250, 0, 285, CHAR_HEIGHT, RA8875_BLACK);  // Increased rectangle size to full erase value
  tft.setCursor(257, 1);
  tft.print(prompt);
  tft.setCursor(440, 1);
  if(abs(startValue) > 2) {
    tft.print(startValue, 0);
  } else {
    tft.print(startValue, 3);
  }

  if(menuEncoderMove != 0) {
    currentValue += menuEncoderMove * increment;  // Bump up or down...
    if(currentValue < minValue)
      currentValue = minValue;
    else if(currentValue > maxValue)
      currentValue = maxValue;

    tft.setCursor(440, 1);
    if(abs(startValue) > 2) {
      tft.print(startValue, 0);
    } else {
      tft.print(startValue, 3);
    }
    menuEncoderMove = 0;
  }

  getEncoderValueFlag = false;
  return currentValue;
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
    if(ft8MsgSelectActive) {
      if(num_decoded_msg > 0) {
        activeMsg += menuEncoderMove;
        if(activeMsg >= num_decoded_msg) {
          activeMsg = 0;
        } else {
          if(activeMsg < 0) {
            activeMsg = num_decoded_msg - 1;
          }
        }
      }
      menuEncoderMove = 0;
    } else {
      if(bands[currentBand].demod == DEMOD_NFM && nfmBWFilterActive) {
        // we're adjusting NFM demod bandwidth
        filter_pos_BW = last_filter_pos_BW - 5 * menuEncoderMove;
      } else {
        // we're adjusting audio spectrum filter
        posFilterEncoder = lastFilterEncoder - 5 * menuEncoderMove;
      }
    }
  }
}
