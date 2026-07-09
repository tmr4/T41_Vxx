// generic calibration routines

#include "SDT.h"

#include "ButtonProc.h"
#include "Display.h"


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
  ChangeMode(CAL_MODE);
  ClearScreen();

  t41.DisplayState = DISPLAY_CALIBRATION;
  SetInfoBoxWindow(3);

  switch(calType) {
    case 0:  // Freq
      //CalibrateFrequency();
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
FLASHMEM void CalibrateFrequency() {
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
