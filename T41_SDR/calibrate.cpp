// generic calibration routines

//#include <LinearRegression.h>      // https://github.com/cubiwan/Regressino/
#include <Linear2DRegression.hpp>  // https://github.com/nkaaf/Arduino-Regression

#include "SDT.h"

#include "ButtonProc.h"
#include "calibrate.h"
#include "Demod.h"
#include "Display.h"
#include "Process.h"
#include "Tune.h"

//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

int calibrateItem = -1;

// preserve/restore radio state
int userRadioState, userRadioMode, userDemodMode, userDisplayState;
int userActiveBand, userTxPower;
int userCenterFreq, userNCOFreq, userFilterHiCut, userFilterLoCut;
int userFreqSpecScale, userSpectrumZoom, userAudioVolume;
float userIQAmpFactor, userIQPhaseFactor;


//-------------------------------------------------------------------------------------------------------------
// Forwards
//-------------------------------------------------------------------------------------------------------------

bool CalibrateFrequency(bool startFlag);
void SetBPFBand(int currentBand);

//-------------------------------------------------------------------------------------------------------------
// Code
//-------------------------------------------------------------------------------------------------------------

//-------------------------------------------------------------------------------------------------------------
// Initialization and Restoration
//-------------------------------------------------------------------------------------------------------------

/*****
  Purpose: save radio state
  *** TODO: validate this covers all state variables for all calibration types ***
  *** TODO: consider moving to init function if not called separately anywhere ***
 *****/
FLASHMEM void SaveRadioState() {
  userRadioState = t41.RadioState;
  userRadioMode = t41.RadioMode;
  userDemodMode = t41.DemodMode;
  userDisplayState = t41.DisplayState;

  userActiveBand = t41.ActiveBand;
  userTxPower = t41.TxPower;

  userCenterFreq = t41.CenterFreq;
  userNCOFreq = t41.NCOFreq;
  userFilterHiCut = t41.FilterHiCut;
  userFilterLoCut = t41.FilterLoCut;

  userFreqSpecScale = t41.FreqSpecScale;
  userSpectrumZoom = t41.SpectrumZoom;
  userAudioVolume = t41.AudioVolume;
}

/*****
  Purpose: restore radio state after IQ calibrations
  *** TODO: validate proper restoration for all calibration types ***
 *****/
FLASHMEM void RestoreRadioState() {
  int volSetting = 0;

  t41.RadioState = RECONFIGURE_STATE;
  ChangeMode(userRadioMode, userDemodMode);
  t41.DisplayState = userDisplayState;

  if(t41.ActiveBand != userActiveBand) {
    ChangeBand(userActiveBand - t41.ActiveBand);
    t41.ActiveBand = userActiveBand;
  }
  t41.TxPower = userTxPower;


  t41.CenterFreq = userCenterFreq;
  t41.NCOFreq = userNCOFreq;
  t41.FilterHiCut = userFilterHiCut;
  t41.FilterLoCut = userFilterLoCut;

  t41.FreqSpecScale = userFreqSpecScale;
  t41.SpectrumZoom = userSpectrumZoom;
  volSetting = userAudioVolume;

  // slowly raise volume during calibration to avoid artifacts
  while(volSetting > 0) {
    if(t41.AudioVolume < volSetting) {
      t41.AudioVolume++;
    } else {
      volSetting = 0;
    }
    delay(10);
  }
}

/*****
  Purpose: exit calibration mode
  *** TODO: validate this covers all state variables for all calibration types ***
 *****/
FLASHMEM void CalibrationExit() {

  ClearScreen();
  RestoreRadioState();

  // Restore the user's zoom setting
  //SetZoom(userSpectrumZoom); // ... and zoom display

  //SetFreq(t41.CenterFreq);

  // reset frequency spectrum buffers
  //InitFFTArrays();

  digitalWrite(RXTX, LOW);  // Turn off the transmitter.

  t41.DisplayState = DISPLAY_T41;

  SetInfoBoxWindow(0);

  // restore screen
  RedrawDisplayScreen();
}

/*****
  Purpose: perform common calibration initialization tasks
  *** TODO: validate this covers all state variables for all calibration types ***
 *****/
FLASHMEM void CalibrationInit(int calType) {
  SaveRadioState();
  //ChangeMode(CAL_MODE);

  //ClearScreen();
  //t41.DisplayState = DISPLAY_CALIBRATION;
  //SetInfoBoxWindow(3);

  switch(calType) {
    case 0:  // Freq
      t41.RadioState = FREQ_CAL_STATE;
      ChangeMode(CAL_MODE, DEMOD_SAM);
      //ChangeMode(DSB_MODE, DEMOD_SAM);
#ifdef USE_BPF_BOARD
      SetBPFBand(-1);
#endif
      t41.ResetFreq(5000000);
      SetupBandFreq(t41.CenterFreq);
      CalibrateFrequency(true);
      break;

    case 1:  // RX IQ
      break;

    case 2:  // TX IQ
      break;

    case 3: // Two Tone
      break;

    case 4: // CW Pwr
      break;

    case 5: // SSB Pwr
      break;

    case 6: // cancel
    default:
      break;
  }
}

//-------------------------------------------------------------------------------------------------------------
// Calibration Routines
//-------------------------------------------------------------------------------------------------------------

/*****
  Determine frequency correction factor for SI5351

  Set up prior to IQ calibrations.
 *****/
FLASHMEM bool CalibrateFrequency(bool startFlag) {
  const int freqAutoLowSet = 500;
  const int freqAutoIncrementSet = 100;
  bool completeFlag = false;

  static Linear2DRegression *corrFacReg;
  static int freqCalFactorStart = 0;
  static int autoCount = 0;
  //static int autoCalOffset = 0;
  static int correctionFactor = freqCorrectionFactor;
  static long last = 0;

  if(startFlag) {
    // start auto frequency calibration
    Serial.printf("Performing Frequency Calibration\nCurrent factor: %d\n.", freqCorrectionFactor);

    corrFacReg = new Linear2DRegression();
    corrFacReg->reset();

    // set first auto calc point
    freqCalFactorStart = freqCorrectionFactor;
    autoCount = 0;

    freqCorrectionFactor = freqCalFactorStart - freqAutoLowSet + autoCount * freqAutoIncrementSet;
    //autoCalOffset = -freqAutoLowSet + autoCount * freqAutoIncrementSet;
    SetSI5351FreqCorFactor(freqCorrectionFactor);
    last = millis();
  } else {
    // process IQ data and calc frequency error each loop
    //YieldToProcess();

    // update frequency error every 5 seconds
    if(millis() - last >= 5000) {
      // update correction factor regression and display
      corrFacReg->addPoint(GetSAMFreqError(), freqCorrectionFactor);
      correctionFactor = corrFacReg->calculate(0);
      Serial.printf("SAM Error: %d, New factor: %d\n", (int)(GetSAMFreqError()*10.0), freqCorrectionFactor);

      autoCount++; // increment auto plot counter
      freqCorrectionFactor = freqCalFactorStart - freqAutoLowSet + autoCount * freqAutoIncrementSet;
      //autoCalOffset = -freqAutoLowSet + autoCount * freqAutoIncrementSet;

      SetSI5351FreqCorFactor(freqCorrectionFactor);

      //Serial.print(".");
      last = millis();
    }

    if(autoCount >= 11) {
      // auto mode complete, clean up
      delete corrFacReg;

      freqCorrectionFactor = correctionFactor;
      Serial.printf("\nNew factor: %d\n", freqCorrectionFactor);
      completeFlag = true;
    }
  }

  return completeFlag;
}

/*****
  Combined input/output to calibrate the receive IQ
 *****/
FLASHMEM void CalibrateReceiveIQ() {
}

/*****
  Combined input/output to calibrate the transmit IQ
 *****/
FLASHMEM void CalibrateTransmitIQ() {
}

/*****
  Digital two tone test
 *****/
FLASHMEM void TwoToneTest() {
}
