
#include "SDT.h"

#include "Display.h"
#include "Encoders.h"
#include "Filter.h"
#include "Menu.h"
#include "MenuProc.h"
#include "Tune.h"

//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

int TxRxFreq, NCOFreq;

bool splitVFO;

int CWFreqShift = 750;
//int CWFreqShift = 0;

//-------------------------------------------------------------------------------------------------------------
// Code
//-------------------------------------------------------------------------------------------------------------

/*****
  Purpose: Set center tuning frequency
           NCOFreq is unchanged
*****/
void SetTxRxFreq(int freq) {
  TxRxFreq = freq;

  SetFreq();

  switch(displayState) {
    case DISPLAY_T41:
      ShowFrequency();          // update frequency display
      ShowOperatingStats();     // update center frequency in band info
      ShowSpectrumFreqValues(); // update spectrum frequency values
      break;

    case DISPLAY_BEACON_MONITOR:
      break;

    case DISPLAY_CALIBRATION:
      ShowOperatingStats();

      if(calibrateItem == 1) {
        // receive IQ calibration
        ShowSpectrumFreqValues();
      }
      break;

    case DISPLAY_FULL_MENU:
      ShowFrequency();
      ShowOperatingStats();
      break;

    default:
    // no screen updates at all
    break;
  }
}

/*****
  Purpose: Reset tuning to center
           NCOFreq is set to zero

  Parameter list:
  void

  Return value:
  void
*****/
void ResetTuning() {
  centerFreq += NCOFreq;
  NCOFreq = 0L;

  SetTxRxFreq(centerFreq);

  switch(displayState) {
    case DISPLAY_T41:
      ShowFrequency();          // update frequency display
      ShowOperatingStats();     // update center frequency in band info
      ShowSpectrumFreqValues(); // update spectrum frequency values
      break;

    case DISPLAY_BEACON_MONITOR:
      break;

    case DISPLAY_FULL_MENU:
      ShowFrequency();
      ShowOperatingStats();
      break;

    default:
    // no screen updates at all
    break;
  }
  DrawBandwidthBar();
}

/*****
  Purpose: Adjust center tuning frequency
           NCOFreq is unchanged

  Parameter list:
    long tuneChange - amound to change center freq
*****/
void SetCenterTune(int tuneChange) {
  centerFreq += tuneChange;  // tune the master vfo

  SetTxRxFreq(centerFreq + NCOFreq);
}

/*****
  Purpose: Set NCO frequency
*****/
void SetNCOFreq(int newNCOFreq) {
  int lowSideAdj = 0, highSideAdj = 0;

  switch(currentDemodMode) {
    case DEMOD_USB:
    case DEMOD_PSK31_WAV:
    case DEMOD_PSK31:
    case DEMOD_FT8:
    case DEMOD_FT8_WAV:
      lowSideAdj = 0;
      highSideAdj = currentFilterHiCut;
      break;

    case DEMOD_LSB:
      lowSideAdj = currentFilterHiCut;
      highSideAdj = 0;
      break;

    case DEMOD_AM:
    case DEMOD_SAM:
      break;

    case DEMOD_NFM:
      break;
  }

  NCOFreq = newNCOFreq;
  fineTuneFlag = true;
  if(activeVFO == VFO_A) {
    currentFreqA = centerFreq + NCOFreq;
  } else {
    currentFreqB = centerFreq + NCOFreq;
  }

  // recenter at band edges
  if(spectrumZoom != 0) {
    if((NCOFreq + highSideAdj) >= (sampleRate / 2.0 / (1 << spectrumZoom))) {
      NCOFreq += highSideAdj;
      fineTuneFlag = false;
      resetTuningFlag = true;
      return;
    }
    if((NCOFreq - lowSideAdj) <= (-sampleRate / 2.0 / (1 << spectrumZoom))) {
      NCOFreq -= lowSideAdj;
      fineTuneFlag = false;
      resetTuningFlag = true;
      return;
    }
  } else if(NCOFreq > 142000 || NCOFreq < -43000) {  // Offset tuning window in zoom 1x
    fineTuneFlag = false;
    resetTuningFlag = true;
    return;
  }

  TxRxFreq = centerFreq + NCOFreq;
}

/*****
  Purpose: Set fine tuning frequency

  int tuneChange: the amount to increment/decrement the tuned frequency
*****/
void SetFineTune(int tuneChange) {
  SetNCOFreq(NCOFreq + tuneChange);
}


// *** TODO: display dependent ***
FLASHMEM void SplitVFOFollowup() {
  // *** TODO: need to reestablish "Split Active" that didn't work in ver49.2k ***
  //tft.setTextColor(RA8875_RED);
  //tft.setCursor(FILTER_PARAMETERS_X + 180, FILTER_PARAMETERS_Y + 6);
  //tft.print("Split Active");
  //splitVFO = true;
}

/*****
  Purpose: Set VFO A to receive frequency and VFO B to the transmit frequency
*****/
FLASHMEM void DoSplitVFO() {
  currentFreqB = currentFreqA;

  // GetMenuValue(minValue, maxValue, startValue, increment, prompt, valueOffset)
  //GetMenuValue(-40, 30, &currentFreqB, SPLIT_INCREMENT, "Xmit offset:", 200, NULL, NULL, &SplitVFOFollowup);
  GetMenuValue(-40, 30, &currentFreqB, 500, "Xmit offset:", 200, NULL, NULL, &SplitVFOFollowup);
}
