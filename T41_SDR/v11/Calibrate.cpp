// v11 specific calibration file

#include "..\SDT.h"

#include "..\AudioConfig.h"
#include "..\Button.h"
#include "..\ButtonProc.h"
#include "..\Display.h"
#include "..\EEPROM.h"
#include "..\Encoders.h"
#include "..\Exciter.h"
#include "..\FIR.h"
#include "..\Menu.h"
#include "..\MenuProc.h"
#include "..\Process.h"
#include "..\Tune.h"
#include "..\t41Control.h"
#include "..\Utility.h"

// for v11 only
// Prerequisite: QSD board RF in connected to QSE RF out with 20 dB attenuator

//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

#define GAIN_COARSE_MAX 1.2
#define GAIN_COARSE_MIN 0.8
#define PHASE_COARSE_MAX 0.2
#define PHASE_COARSE_MIN -0.2

#define GAIN_COARSE_STEP2_N 10
#define PHASE_COARSE_STEP2_N 10
#define GAIN_FINE_N 5
#define PHASE_FINE_N 5

#define SIG_STRENGTH_MAX 8

// preserve/restore radio state
int userFilterLowCut, userFilterHiCut, userMode, userDemodMode, userRadioState;
int userScale, userZoomIndex, userXmtMode, userBand;
long userCenterFreq, userTxRxFreq, userNCOFreq;
float userIQAmpFactor, userIQPhaseFactor;

// common to several routines
float iqIncrementValues[] = { 0.001, 0.01, 0.1 };
int iqIncrementIndex = 1;
float iqCorInc = iqIncrementValues[iqIncrementIndex];
int iqCorIncY = 115;
int menuX = 530;

float minSignalStrength;
bool transmitCal; // calibration mode: true=transmit, false=receive

// frequency calibration

// receive calibration

// transmit calibration
float plotValue = 0;
bool plotValueInc = true; // true = 1.0, false = 0.1
int signalStrengthSource = 0; // signal strength source: 0 = manual user entry, 1 = external via CAT SM command, 2 = internal loopback
const char *signalStrengthSources[3] =  {"man", "ext", "loop"};

// two tone variables


int transmitPowerLevelTemp;

float32_t sinBuffer3[256];
float32_t cosBuffer3[256];

// delete when ready
int calTypeFlag = 0;

//-------------------------------------------------------------------------------------------------------------
// Forwards
//-------------------------------------------------------------------------------------------------------------

void SetFreqCal(long calFreqShift);

void DrawIQGainPlot();

//-------------------------------------------------------------------------------------------------------------
// Code
//-------------------------------------------------------------------------------------------------------------

/*****
  Purpose: save common radio state parameters, change center frequency to current frequancy,
           and configure audio state

           *** radioState may occasionally be referenced by various global routines use
               during calibration but has no other effect as the calibration routines all
               bypass the main processing loop ***

  Parameter List:
    calType - type of calibration we're cleaning up after
    rState  - radioState for calibration
    aState  - audio configuration state for calibration
 *****/
FLASHMEM void CalibratePreamble(int calType, int rState, int aState) {
  // Save the current operating state to restore later
  userTxRxFreq = TxRxFreq;
  userCenterFreq = centerFreq;
  userNCOFreq = NCOFreq;
  userRadioState = radioState;
  userMode = radioMode;
  userDemodMode = bands[currentBand].demod;
  userZoomIndex = spectrumZoom;

  TxRxFreq = centerFreq = centerFreq + NCOFreq;
  NCOFreq = 0;

  // calibration specific configuration
  switch(calType) {
    case 0: // frequency cal
      break;

    case 1: // receive IQ cal
      //if(bands[currentBand].demod == DEMOD_LSB) SetFreqCal(24000 - 2000);
      //if(bands[currentBand].demod == DEMOD_USB) SetFreqCal(24000 + 2250);
      //SetFreqCal(1500);
      //SetFreqCal(12000);
      SetFreqCal(22000);
      //SetFreqCal(24000);
      //SetFreqCal(24000 + 2250); // puts wanted sideband at proper v11 place (blue bar) but unwanted shifted left
      //SetFreqCal(0);
      SetZoom(0); // 1x
      //SetZoom(1); // 2x
      //SetZoom(2); // 4x
      userIQAmpFactor = IQAmpCorrectionFactor[currentBand];
      userIQPhaseFactor = IQPhaseCorrectionFactor[currentBand];
      break;

    case 2: // transmit IQ cal
      //SetFreqCal(750);
      //SetFreqCal(1500);
      SetFreqCal(0);
      SetZoom(2); // 4x
      //SetZoom(3); // 8x
      //SetZoom(1); // 2x
      userIQAmpFactor = IQXAmpCorrectionFactor[currentBand];
      userIQPhaseFactor = IQXPhaseCorrectionFactor[currentBand];
      break;

    case 3: // two tone test
    default:
      break;
  }

  for(int i = 0; i < 256; i++) {
    // used in calibration
    float theta = i * 2.0 * PI * 3000.0 / 24000.0;
    cosBuffer3[i] = cos(theta);
    sinBuffer3[i] = sin(theta);
    //cosBuffer3[i] = 0.0;
    //sinBuffer3[i] = 0.0;
  }




  transmitPowerLevelTemp = transmitPowerLevel;
  //transmitPowerLevel = 5;
  transmitPowerLevel = 1;
  //powerOutCW[currentBand] = (-.0133 * transmitPowerLevel * transmitPowerLevel + .7884 * transmitPowerLevel + 4.5146) * CWPowerCalibrationFactor[currentBand];

  //digitalWrite(MUTE, HIGH);  //  Mute Audio  (HIGH=Mute)
  digitalWrite(RXTX, HIGH);  // Turn on transmitter.




  // general calibration configuration
  radioState = rState;
  ConfigAudioState(aState);
  //SetFreq(); // v12
}

/*****
  Purpose: Shut down and clean up after IQ calibrations.  New function.  KF5N August 14, 2023
 *****/
FLASHMEM void CalibratePost(int calType) {
  // restore radio operating state
  TxRxFreq = userTxRxFreq;
  NCOFreq = userNCOFreq;
  centerFreq = userCenterFreq;
  radioState = userRadioState;
  bands[currentBand].demod = userDemodMode;
  spectrumZoom = userZoomIndex;

  // calibration specific restoration
  switch(calType) {
    case 0: // frequency cal
      bands[currentBand].demod = userDemodMode;
      //currentFilterLoCut = userFilterLowCut;
      //currentFilterHiCut = userFilterHiCut;
      //CalcFilters();
      break;

    case 1: // receive cal
      if(currentBand != userBand) {
        ChangeBand(userBand - currentBand);
        currentBand = userBand;
      }

      SetFreq();
      currentScale = userScale;

      // reset frequency spectrum buffers
      SET_VAR(pixelnew, SPECTRUM_BOTTOM);
      InitFFTArrays();
      newSpectrumFlag = 0;
      break;

    case 2: // transmit cal
      // reset frequency spectrum buffers
      SET_VAR(pixelnew, SPECTRUM_BOTTOM);
      InitFFTArrays();
      newSpectrumFlag = 0;
      break;

    case 3: // two tone test
      break;

    default:
      break;
  }

  digitalWrite(RXTX, LOW);  // Turn off the transmitter.

  transmitPowerLevel = transmitPowerLevelTemp;  // Restore the user's transmit power level setting.  KF5N August 15, 2023

  // Restore the user's zoom setting
  SetZoom(userZoomIndex); // ... and zoom display

  // restore screen
  tft.writeTo(L2);
  tft.clearMemory();
  tft.writeTo(L1);
  tft.clearMemory();

  RedrawDisplayScreen();

  lastState = -1; // force radio state reset
}

FLASHMEM void DisplayIQAdjustIncrement(int adjChars) {
  int adjX;

  tft.setFontScale((enum RA8875tsize)0);
  adjX = adjChars * tft.getFontWidth();
  tft.fillRect(menuX + adjX, iqCorIncY, 50, tft.getFontHeight(), RA8875_BLACK);
  tft.setTextColor(RA8875_GREEN);
  tft.setCursor(menuX + adjX, iqCorIncY);
  tft.print(iqCorInc, 3);
}

FLASHMEM void UpdateIQDisplay(bool autoFlag = false) {
  tft.fillRect(680, 320, 150, CHAR_HEIGHT, RA8875_BLACK);
  tft.setFontScale((enum RA8875tsize)1);
  if(autoFlag) {
    tft.setTextColor(RA8875_YELLOW);
  } else {
    tft.setTextColor(RA8875_GREEN);
  }
  tft.setCursor(680, 320);
  if(transmitCal) {
    tft.print(IQXAmpCorrectionFactor[currentBand], 3);
  } else {
    tft.print(IQAmpCorrectionFactor[currentBand], 3);
  }
  tft.fillRect(680, 360, 150, CHAR_HEIGHT, RA8875_BLACK);
  tft.setCursor(680, 360);
  if(transmitCal) {
    tft.print(IQXPhaseCorrectionFactor[currentBand], 3);
  } else {
    tft.print(IQPhaseCorrectionFactor[currentBand], 3);
  }
}

FLASHMEM bool AdjustIQFactors() {
  bool adjustFlag = false;

  // IQ amp correction factor
  if(menuEncoderMove != 0) {
    if(transmitCal) {
      IQXAmpCorrectionFactor[currentBand] += menuEncoderMove * iqCorInc;
    } else {
      IQAmpCorrectionFactor[currentBand] += menuEncoderMove * iqCorInc;
    }

    menuEncoderMove = 0;
    adjustFlag = true;
  }

  // IQ phase correction factor
  if(adjustVolEncoder != 0) {
    if(transmitCal) {
      IQXPhaseCorrectionFactor[currentBand] += adjustVolEncoder * iqCorInc;
    } else {
      IQPhaseCorrectionFactor[currentBand] += adjustVolEncoder * iqCorInc;
    }

    adjustVolEncoder = 0;
    adjustFlag = true;
  }

  if(adjustFlag) {
    UpdateIQDisplay();
  }
  return adjustFlag;
}

FLASHMEM bool AdjustRxTxAtten(bool receiveFlag = true) {
  return true;
}

FLASHMEM void AdjustCalFactors() {
  AdjustIQFactors();
  AdjustRxTxAtten();

  // adjust image value
  EncoderCenterTune();
  if(tuneChange != 0) {
    plotValue += tuneChange * (plotValueInc ? 1.0 : 0.1);
    tuneChange = 0;

    tft.setFontScale((enum RA8875tsize)1);
    tft.fillRect(680, 440, 150, tft.getFontHeight(), RA8875_BLACK);
    tft.setTextColor(RA8875_GREEN);
    tft.setCursor(680, 440);
    tft.print(plotValue, 1);
  }
}

FLASHMEM void PrintAtten() {
}

//-------------------------------------------------------------------------------------------------------------
// Menus and Displays
//-------------------------------------------------------------------------------------------------------------

FLASHMEM void ShowCalibrationMenu() {
  int menuY = iqCorIncY - 30;

  tft.setFontScale((enum RA8875tsize)0);
  tft.setTextColor(RA8875_YELLOW);
  tft.setCursor(menuX, menuY);
  tft.print("9: Plot value");
  menuY += 15;
  tft.setCursor(menuX, menuY);
  tft.print("10: Plot val inc");
  menuY += 15;
  tft.setCursor(menuX, menuY);
  tft.print("11: IQ inc");
  menuY += 15;
  tft.setCursor(menuX, menuY);
  tft.print("12: Sig Source");
  menuY += 15;
  tft.setCursor(menuX, menuY);
  tft.print("13: All bands cal");
  menuY += 15;
  tft.setCursor(menuX, menuY);
  tft.print("14: Auto cal");
  menuY += 15;
  tft.setCursor(menuX, menuY);
  tft.print("15: Attn In/Out toggle");
  menuY += 15;
  //tft.setCursor(menuX, menuY);
  //tft.print("16: Toggle directions");
  //menuY += 15;
  tft.setCursor(menuX, menuY);
  tft.print("17: Cancel");
  menuY += 15;
  tft.setCursor(menuX, menuY);
  tft.print("Select: Save/Exit");
  menuY += 15;
  tft.setCursor(menuX, menuY);
  tft.print("Band+/-: Change band");
  menuY += 15;
  tft.setCursor(menuX, menuY);
  tft.print("Encoders:");
  menuY += 15;
  tft.setCursor(menuX, menuY);
  tft.print("Filter: Gain adj");
  menuY += 15;
  tft.setCursor(menuX, menuY);
  tft.print("Vol: Phase adj");
  menuY += 15;
  //tft.setCursor(menuX, menuY);
  //tft.print("Fine: Atten adj");
  //menuY += 15;
  tft.setCursor(menuX, menuY);
  tft.print("Center: Sig str adj");

  // display IQ adjust increment
  DisplayIQAdjustIncrement(20);
}

FLASHMEM void ShowCalibrationDisplay() {
  ClearScreen();

  // display instructions
  tft.setFontScale((enum RA8875tsize)1);
  tft.setTextColor(RA8875_WHITE);
  tft.setCursor(100, 10);
  if(transmitCal) {
    tft.print("Calibrate TX IQ");
  } else {
    tft.print("Calibrate RX IQ");
  }

  ShowCalibrationMenu();

  // display IQ factors and image value
  tft.setFontScale((enum RA8875tsize)1);
  tft.setTextColor(RA8875_WHITE);
  tft.setCursor(menuX, 320);
  tft.print("IQ Gain");
  tft.setCursor(menuX, 360);
  tft.print("IQ Phase");

  tft.setCursor(menuX, 400);
  tft.print("Sig Sup");
  tft.setCursor(menuX, 440);
  tft.print("Plot Val");
  tft.fillRect(680, 440, 150, tft.getFontHeight(), RA8875_BLACK);
  tft.setTextColor(RA8875_GREEN);
  tft.setCursor(680, 440);
  tft.print(plotValue, 1);

  UpdateIQDisplay();

  DrawIQGainPlot();
}

FLASHMEM void UpdateCalDisplayData() {
  tft.setFontScale((enum RA8875tsize)1);
  tft.fillRect(680, 400, 150, tft.getFontHeight(), RA8875_BLACK);
  tft.fillRect(680, 440, 150, tft.getFontHeight(), RA8875_BLACK);
  tft.setTextColor(RA8875_YELLOW);
  tft.setCursor(680, 400);
  tft.print(signalStrength, 1);
  tft.setCursor(680, 440);
  tft.print(minSignalStrength, 1);

  UpdateIQDisplay(true);
}

//-------------------------------------------------------------------------------------------------------------
// Signals and Yield
//-------------------------------------------------------------------------------------------------------------

FLASHMEM void PrepareExciterIQDataCal(int mode) {
  // apply any mode specific processing
  switch(mode) {
    case 0:
      // Two-tone signal generation - uses Hilbert transfor to generate IQ signals
      break;

    case 1: // receive/transmit calibration
      //arm_scale_f32(sinBuffer3, 0.5, audioBufferL_EX, 256);
      //arm_scale_f32(cosBuffer3, 0.5, audioBufferR_EX, 256);
      //arm_scale_f32(sinBuffer3, 0.05, audioBufferL_EX, 256);
      //arm_scale_f32(cosBuffer3, 0.05, audioBufferR_EX, 256);
      //arm_scale_f32(sinBuffer3, 0.01, audioBufferL_EX, 256);
      //arm_scale_f32(cosBuffer3, 0.01, audioBufferR_EX, 256);
      arm_scale_f32(sinBuffer3, 0.005, audioBufferL_EX, 256);
      arm_scale_f32(cosBuffer3, 0.005, audioBufferR_EX, 256);
      //arm_scale_f32(sinBuffer3, 0.001, audioBufferL_EX, 256);
      //arm_scale_f32(cosBuffer3, 0.001, audioBufferR_EX, 256);


      //arm_scale_f32(sinBuffer3, 0.2, audioBufferL_EX, 256);
      //arm_scale_f32(cosBuffer3, 0.2, audioBufferR_EX, 256);


      //arm_scale_f32(cosBuffer3, 0.005, audioBufferL_EX, 256);
      //arm_scale_f32(sinBuffer3, 0.005, audioBufferR_EX, 256);
      //arm_scale_f32(cosBuffer3, 1.005, audioBufferL_EX, 256);
      //arm_scale_f32(sinBuffer3, 1.005, audioBufferR_EX, 256);

      PlayExciterIQData();
      break;

    // Passthrough
    case 2:
    default:
      break;
  }
}

//-------------------------------------------------------------------------------------------------------------
// Plot Routines
//-------------------------------------------------------------------------------------------------------------
/*
FASTRUN void PlotSpectrum(int *calBins, int binSize) {
  int yPlot, y1Plot;
  static int yOldPlot[SPECTRUM_RES];
  static int yOldAudioPlot[AUDIO_SPEC_RES];
  static int currentNF = 0;

  YieldToProcess(true);

  // initialize yOldPlot if this is a new spectrum
  // otherwise copy y values from last loop
  if(newSpectrumFlag == 0) {
    memset(yOldPlot, SPECTRUM_BOTTOM, SPECTRUM_RES * sizeof(int));
    newSpectrumFlag = 1;
  }

  // Draw the frequency and audio spectrums, gather data for waterfall
  for(int x1 = 0; x1 < SPECTRUM_RES - 1; x1++) {
    bool drawSpec = true, eraseSpec = true, inBoxLow = true, inBoxHigh = true;

    // calculate the freq spectrum plot value; pixelnew spectrum is calculated in CalcZoomFreqSpec
    yPlot = spectrumNoiseFloor - pixelnew[x1] - currentNF;
    y1Plot = spectrumNoiseFloor - pixelnew[x1 + 1] - currentNF;

    // clear erase flag if we don't need to erase anything
    if((yOldPlot[x1] == SPECTRUM_BOTTOM) && (yOldPlot[x1 + 1] == SPECTRUM_BOTTOM)) {
      eraseSpec = false;
    }
    if((yOldPlot[x1] == SPECTRUM_TOP_Y) && (yOldPlot[x1 + 1] == SPECTRUM_TOP_Y)) {
      eraseSpec = false;
    }

    // erase the old spectrum if needed
    if(eraseSpec && (displayState == DISPLAY_T41)) {
      //tft.drawLine(SPECTRUM_LEFT_X + x1, yOldPlot[x1 + 1], SPECTRUM_LEFT_X + x1, yOldPlot[x1], RA8875_BLACK);
    }
    tft.drawLine(SPECTRUM_LEFT_X + x1, yOldPlot[x1 + 1], SPECTRUM_LEFT_X + x1, yOldPlot[x1], RA8875_BLACK);

    // prevent drawing spectrum outside of the spectrum area
    // also clear draw flag if we don't need to draw anything
    if(yPlot > SPECTRUM_BOTTOM) {
      yPlot = SPECTRUM_BOTTOM;
      inBoxLow = false;
    }
    if(y1Plot > SPECTRUM_BOTTOM) {
      y1Plot = SPECTRUM_BOTTOM;
      drawSpec = inBoxLow ? true : false;
    }
    if(yPlot < SPECTRUM_TOP_Y) {
      yPlot = SPECTRUM_TOP_Y;
      inBoxHigh = drawSpec ? false : true;
    }
    if(y1Plot < SPECTRUM_TOP_Y) {
      y1Plot = SPECTRUM_TOP_Y;
      drawSpec = inBoxHigh ? true : false;
    }

    // draw the new spectrum if needed
    if(drawSpec && (displayState == DISPLAY_T41)) {
      //tft.drawLine(SPECTRUM_LEFT_X + x1, y1Plot, SPECTRUM_LEFT_X + x1, yPlot, RA8875_YELLOW);
    }
    tft.drawLine(SPECTRUM_LEFT_X + x1, y1Plot, SPECTRUM_LEFT_X + x1, yPlot, RA8875_YELLOW);

    // save plot value to erase spectrum next loop
    yOldPlot[x1] = yPlot;

    YieldToProcess();
  }

  // save last plot value for erasing on next loop
  yOldPlot[SPECTRUM_RES - 1] = y1Plot;

}
*/

FLASHMEM void PlotSpectrum(int *calBins, int binSize) {
  int16_t yPlot, y1Plot;
  static int yOldPlot[SPECTRUM_RES];
  bool drawSpec = true, eraseSpec = true, inBoxLow = true, inBoxHigh = true;
  int x, y;

  YieldToProcess(true);

  for(int i = 0; i < 2; i++)
  {
    //int i = 0;
    if(i == 0) {
      x = calBins[0] - binSize;
      y = calBins[0] + binSize;
    } else {
      x = calBins[1] - binSize;
      y = calBins[1] + binSize;
    }
    for(int x1 = x; x1 < y; x1++) {
    //for(int x1 = 0; x1 < SPECTRUM_RES - 1; x1++) {
    //for(int x1 = 200; x1 < 300; x1++) {

      // calculate the freq spectrum plot value; pixelnew spectrum is calculated in CalcZoomFreqSpec
      yPlot = spectrumNoiseFloor - pixelnew[x1]; // - currentNF;
      y1Plot = spectrumNoiseFloor - pixelnew[x1 + 1]; // - currentNF;
      //yPlot = spectrumNoiseFloor - pixelnew[x1] + 100;
      //y1Plot = spectrumNoiseFloor - pixelnew[x1 + 1] + 100;
      //yPlot = spectrumNoiseFloor - pixelnew[x1] + 150;
      //y1Plot = spectrumNoiseFloor - pixelnew[x1 + 1] + 150;

      // clear erase flag if we don't need to erase anything
      if((yOldPlot[x1] == SPECTRUM_BOTTOM) && (yOldPlot[x1 + 1] == SPECTRUM_BOTTOM)) {
        eraseSpec = false;
      }
      if((yOldPlot[x1] == SPECTRUM_TOP_Y) && (yOldPlot[x1 + 1] == SPECTRUM_TOP_Y)) {
        eraseSpec = false;
      }

      // erase the old spectrum if needed
      //if(eraseSpec && (displayState == DISPLAY_T41)) {
      //  tft.drawLine(SPECTRUM_LEFT_X + x1, yOldPlot[x1 + 1], SPECTRUM_LEFT_X + x1, yOldPlot[x1], RA8875_BLACK);
      //}
      tft.drawLine(SPECTRUM_LEFT_X + x1, yOldPlot[x1 + 1], SPECTRUM_LEFT_X + x1, yOldPlot[x1], RA8875_BLACK);

      // prevent drawing spectrum outside of the spectrum area
      // also clear draw flag if we don't need to draw anything
      //if(yPlot > SPECTRUM_BOTTOM) {
      //  //Serial.println(yPlot);
      //  yPlot = SPECTRUM_BOTTOM;
      //  inBoxLow = false;
      //}
      //if(y1Plot > SPECTRUM_BOTTOM) {
      //  y1Plot = SPECTRUM_BOTTOM;
      //  drawSpec = inBoxLow ? true : false;
      //}
      //if(yPlot < SPECTRUM_TOP_Y) {
      //  yPlot = SPECTRUM_TOP_Y;
      //  inBoxHigh = drawSpec ? false : true;
      //}
      //if(y1Plot < SPECTRUM_TOP_Y) {
      //  y1Plot = SPECTRUM_TOP_Y;
      //  drawSpec = inBoxHigh ? true : false;
      //}

      // draw the new spectrum if needed
      if(drawSpec && (displayState == DISPLAY_T41)) {
        //tft.drawLine(SPECTRUM_LEFT_X + x1, y1Plot, SPECTRUM_LEFT_X + x1, yPlot, RA8875_YELLOW);
      }
      tft.drawLine(SPECTRUM_LEFT_X + x1, y1Plot, SPECTRUM_LEFT_X + x1, yPlot, RA8875_YELLOW);

      // save plot value to erase spectrum next loop
      yOldPlot[x1] = yPlot;

      PrepareExciterIQDataCal(1);
      YieldToProcess();
    }
  }
  yOldPlot[SPECTRUM_RES - 1] = y1Plot;
  //delay(250);
}


// plotType: type of adjustment: false = phase; true = amplitude
FLASHMEM void PlotIQGainValue(float sigStr, bool plotType = true, bool dbm = false) {
  char msg[60], f1[10], f2[10];
  int color;
  float plotX, plotY, plotValue;
  static float minGainX = 0.0, minGainY = 80.0;
  static float minPhaseX = 0.0, minPhaseY = 80.0;

  //Serial.println("at PlotIQGainValue");
  //Serial.println(sigStr);
  if(dbm) {
    if(sigStr > -73.0) {
      plotValue = (sigStr + 73.0) / 10.0;
    } else {
      plotValue = (sigStr + 127.0) / 6.0;
    }
  } else {
    plotValue = sigStr;
  }

  // map and plot value on graph
  plotY = map((int)(10.0 * plotValue), 0, SIG_STRENGTH_MAX * 10, 440, 200);
  if(plotType) {
    //plotX = map(IQXAmpCorrectionFactor[currentBand], 0.8, 1.2, 40, 440);
    plotX = map(IQXAmpCorrectionFactor[currentBand], GAIN_COARSE_MIN, GAIN_COARSE_MAX, 40, 440);
    color = RA8875_YELLOW;

    if(plotValue < minGainY) {
      minGainX = IQXAmpCorrectionFactor[currentBand];
      minGainY = plotValue;
    }

    dtostrf(minGainX, 5, 3, f1);
    dtostrf(minGainY, 3, 1, f2);

    tft.setFontScale((enum RA8875tsize)0);
    tft.fillRect(0, 150, 60 * tft.getFontWidth(), tft.getFontHeight(), RA8875_BLACK);
    tft.setTextColor(RA8875_YELLOW);
    sprintf(msg, " Gain = %.5s @  %.3s", f1, f2);
    tft.setCursor(0, 150);
  } else {
    plotX = map(IQXPhaseCorrectionFactor[currentBand], PHASE_COARSE_MIN, PHASE_COARSE_MAX, 40, 440);
    color = RA8875_CYAN;

    if(plotValue < minPhaseY) {
      minPhaseX = IQXPhaseCorrectionFactor[currentBand];
      minPhaseY = plotValue;
    }

    dtostrf(minPhaseX, 5, 3, f1);
    dtostrf(minPhaseY, 3, 1, f2);

    tft.setFontScale((enum RA8875tsize)0);
    tft.fillRect(0, 165, 60 * tft.getFontWidth(), tft.getFontHeight(), RA8875_BLACK);
    tft.setTextColor(RA8875_CYAN);
    sprintf(msg, " Phase = %.5s @  %.3s", f1, f2);
    tft.setCursor(0, 165);
  }
  tft.fillCircle(plotX, plotY, 2, color);
  tft.print(msg);

  //Serial.println("at PlotIQGainValue end");
}

FLASHMEM void DrawIQGainPlot() {
  // erase only plot
  tft.fillRect(0, 185, 480, 294, RA8875_BLACK);

  // draw gain correction plot
  tft.drawRect(0, 185, 480, 294, RA8875_GREEN);
  tft.drawFastHLine(40, 430, 400, RA8875_GREEN);
  tft.drawFastVLine(40, 200, 230, RA8875_GREEN);

  tft.setFontScale((enum RA8875tsize)0);
  tft.setTextColor(RA8875_WHITE);
  //tft.setCursor(13, 275);
  tft.setCursor(5, 275);
  tft.print("S");
  tft.setCursor(180, 436);
  tft.print("Gain");
  tft.setCursor(175, 436 + 15);
  tft.print("Phase");
  for(int k = 0; k < 5; k++) {
    tft.drawFastVLine(40 + k * 100, 430, 7, RA8875_GREEN);

    tft.setCursor(30 + k * 100, 438);
    tft.print((float)GAIN_COARSE_MIN + ((float)(GAIN_COARSE_MAX - GAIN_COARSE_MIN)) / 4.0 * (float)k, 1);
    tft.setCursor(30 + k * 100, 438 + 15);
    tft.print((float)PHASE_COARSE_MIN + ((float)(PHASE_COARSE_MAX - PHASE_COARSE_MIN)) / 4.0 * (float)k, 1);
    //tft.setCursor(10, 190 + k * 57.5);
    tft.setCursor(18, 190 + k * 57.5);
    tft.print(SIG_STRENGTH_MAX * (1 - (float)k / 4.0), 0);
    tft.drawFastHLine(33, 200 + k * 57.5, 7, RA8875_GREEN);
  }

  tft.setCursor(0, 135);
  tft.print("Min Points:");
}

//-------------------------------------------------------------------------------------------------------------
// Signal Strength
//-------------------------------------------------------------------------------------------------------------

FLASHMEM void GetSignalStrength(float *pSS, int passes = 0, bool getMeanSS = true) {
  bool result = false;
  int capture_bins = 10;  // sets the number of bins to scan for signal peak
  int cal_bins[2] = {0, 0};
  int16_t adjAmplitude = 0;
  int16_t refAmplitude = 0;
  uint32_t index_of_max;
  int ssIndex = 0;
  const int numSamples = 10;
  float samples[numSamples], stdev;
  int count = 0;

  //Serial.println("at 5");
  //Serial.println();

  // set bin values depending on calibration type
  //  This is the "spectra scanning" for loop.  During calibration, only small areas of the spectrum need to be examined.
  //  If the entire 512 wide spectrum is used, the calibration loop will be slow and unresponsive.
  //  The scanning areas are determined by receive versus transmit calibration, and LSB or USB.  Thus there are 4 different scanning zones.
  //  All calibrations use a 0 dB reference signal and an "undesired sideband" signal which is to be minimized relative to the reference.
  //  Thus there is a target "bin" for the reference signal and another "bin" for the undesired sideband.
  //  The target bin locations are used by the for-loop to sweep a small range in the FFT.  A maximum finding function finds the peak signal strength.
  if(transmitCal) {
    // transmit calibration
    if(bands[currentBand].demod == DEMOD_LSB) {
      //cal_bins[0] = 240;
      //cal_bins[1] = 305;
      cal_bins[0] = 230;
      cal_bins[1] = 295;
    }
    if(bands[currentBand].demod == DEMOD_USB) {
      cal_bins[0] = 209;
      cal_bins[1] = 273;
    }
  } else {
    // receive calibration
    if(bands[currentBand].demod == DEMOD_LSB) {
      //cal_bins[0] = 310;
      //cal_bins[1] = 460;
      //cal_bins[0] = 315;
      //cal_bins[1] = 455;
      cal_bins[0] = 325;
      cal_bins[1] = 445;
    }
    if(bands[currentBand].demod == DEMOD_USB) {
      cal_bins[0] = 65;
      cal_bins[1] = 192;
    }
  }

  PrepareExciterIQDataCal(1);
  long prevUpdate = millis();
  // update frequency spectrum and get average signal strength
  while(!result) {
    if(millis() - prevUpdate > 10) {
      PrepareExciterIQDataCal(1);
      prevUpdate = millis();
    }

    // plot spectrum
    if((passes > 0) && (count++ == passes)) {
      //PrepareExciterIQDataCal(1);
      PlotSpectrum(cal_bins, capture_bins);
      count = 0;
    } else {
      PrepareExciterIQDataCal(1);
      prevUpdate = millis();
      YieldToProcess(true);
    }

    // calculate sideband signal strength
    if(bands[currentBand].demod == DEMOD_LSB) {
      arm_max_q15(&pixelnew[(cal_bins[0] - capture_bins)], capture_bins * 2, &refAmplitude, &index_of_max);
      arm_max_q15(&pixelnew[(cal_bins[1] - capture_bins)], capture_bins * 2, &adjAmplitude, &index_of_max);
    }
    if(bands[currentBand].demod == DEMOD_USB) {
      arm_max_q15(&pixelnew[(cal_bins[0] - capture_bins)], capture_bins * 2, &adjAmplitude, &index_of_max);
      arm_max_q15(&pixelnew[(cal_bins[1] - capture_bins)], capture_bins * 2, &refAmplitude, &index_of_max);
    }

    signalStrength = ((float)adjAmplitude - (float)refAmplitude) / 1.95;

    if(getMeanSS) {
      // calc sideband rejection
      // collect numSamples samples
      samples[ssIndex++] = signalStrength;

      //Serial.print(samples[ssIndex-1]); Serial.print(", ");

      if(ssIndex == numSamples) {
        // we've got all numSamples samples, process
        arm_std_f32(samples, numSamples, &stdev);

        //if(stdev > 1.0) {
        if(stdev > 1.5) {
          Serial.print(stdev); Serial.print(", ");

          // standard deviation is too high, start again
          ssIndex = 0;
        } else {
          arm_mean_f32(samples, numSamples, &signalStrength);
          *pSS = signalStrength;
          result = true;
        }
      }
    } else {
      if(count == 0) break;
    }
  }

  //Serial.println();
  UpdateCalDisplayData();
}

void SetupSignalStrengthSource(int source) {
  unsigned long prevMillis;

  // set up signal strength source
  switch(source) {
    case 0: // manual
      tft.setTextColor(RA8875_WHITE);
      tft.setCursor(menuX, 400);
      tft.print("Sig Sup");
      tft.setCursor(menuX, 440);
      tft.print("Plot Val");
      //tft.fillRect(680, 440, 150, tft.getFontHeight(), RA8875_BLACK);
      PrintAtten(); // v12
      minSignalStrength = 0;
      break;

    case 1: // external
      // set up this and external unit for calibration
      minSignalStrength = 0;
      signalStrengthSource = 1;
      SendSetFreq(centerFreq + intermediateFreq);
      if(bands[currentBand].demod == DEMOD_LSB) {
        SendSetMode(DEMOD_USB);
      } else {
        SendSetMode(DEMOD_LSB);
      }
      SendSetDisplayZoom(2);
      SendSetNarrowFilter();

      // allow frequency to stabilize
      prevMillis = millis();
      while(millis() - prevMillis < 5000) {
        PrepareMicExciterData();
        T41ControlLoop();
      }

      tft.fillRect(menuX, 400, 150, tft.getFontHeight(), RA8875_BLACK);
      tft.fillRect(menuX, 440, 150, tft.getFontHeight(), RA8875_BLACK);
      tft.setCursor(menuX, 400);
      tft.print("Sig Sup");
      tft.setCursor(menuX, 440);
      tft.print("Max Sup");

      UpdateCalDisplayData();
      break;

    case 2: // loopback
    default:
      break;
  }
}

//-------------------------------------------------------------------------------------------------------------
// Auto Calibration
//-------------------------------------------------------------------------------------------------------------

FLASHMEM void TuneCalParameter(int indexStart, int indexEnd, float increment, float *IQCorrectionFactor) {
  int minIndex = 0;
  int index = indexStart;
  float correctionFactor = *IQCorrectionFactor;
  //unsigned long prevMillis = millis();
  float meanSignalStrength;

  // reset globals
  *IQCorrectionFactor = correctionFactor + index * increment;
  signalStrengthReceivedIndex = -1;
  signalStrengthReceived = false;
  while(index < indexEnd) {
    // *** calibration times ***
    // Mode: loopback
    //  cal type  w/o spectrum    w/ spectrum
    //  transmit  ~30 sec         ~5 min
    //  receive
    //GetSignalStrength(&meanSignalStrength); // w/o spectrum
    GetSignalStrength(&meanSignalStrength, 1); // with spectrum

    if(meanSignalStrength < minSignalStrength) {
      minSignalStrength = meanSignalStrength;
      minIndex = index;
      //Serial.println();
      Serial.print("min factor "); Serial.print(correctionFactor + index * increment); Serial.print(" found @: "); Serial.println(minSignalStrength);
    } else {
      //Serial.println(minSignalStrength);
    }

    PlotIQGainValue(meanSignalStrength, IQCorrectionFactor == &IQXAmpCorrectionFactor[currentBand], true);

    // update IQ correction factor for next increment
    index++;
    *IQCorrectionFactor = correctionFactor + index * increment;
    UpdateCalDisplayData();
  }

  *IQCorrectionFactor = correctionFactor + minIndex * increment;
  UpdateCalDisplayData();
}

FLASHMEM void AutoTune(float *amp, float *phase,
                float gain_coarse_max, float gain_coarse_min,
                float phase_coarse_max, float phase_coarse_min,
                int gain_coarse_step2_N, int phase_coarse_step2_N,
                int gain_fine_N, int phase_fine_N, bool phase_first) {

  *amp = 1.0;
  *phase = 0.0;

  ShowTransmitReceiveStatus();

  tft.setFontScale((enum RA8875tsize)1);
  tft.fillRect(menuX, 240, 250, tft.getFontHeight(), RA8875_BLACK);
  tft.fillRect(menuX, 280, 250, tft.getFontHeight(), RA8875_BLACK);
  tft.fillRect(menuX, 400, 250, tft.getFontHeight(), RA8875_BLACK);
  tft.fillRect(menuX, 440, 250, tft.getFontHeight(), RA8875_BLACK);
  tft.setTextColor(RA8875_WHITE);
  tft.setCursor(menuX, 240);
  tft.print("Auto Xmit Cal");
  tft.setCursor(menuX, 400);
  tft.setTextColor(RA8875_YELLOW);
  tft.print("Sig Sup");
  tft.setCursor(menuX, 440);
  tft.print("Max Sup");
  tft.setFontScale((enum RA8875tsize)0);
  tft.setCursor(menuX, 280);
  tft.print("  starting auto cal...");
  tft.setTextColor(RA8875_YELLOW);
  tft.setCursor(menuX, 295);
  tft.print("  (press 17 to cancel)");

  UpdateCalDisplayData();

  if(phase_first) {
    // Step 2: phase changes in 0.01 steps from -0.2 to 0.2. Find the minimum.
    int phaseStepsCoarseN = (int)((phase_coarse_max - phase_coarse_min) / 0.01 / 2);
    TuneCalParameter(-phaseStepsCoarseN, phaseStepsCoarseN + 1, 0.01, phase);

    // Step 1: Gain in 0.01 steps from 0.5 to 1.5
    int gainStepsCoarseN = (int)((gain_coarse_max - gain_coarse_min) / 0.01 / 2);
    TuneCalParameter(-gainStepsCoarseN, gainStepsCoarseN + 1, 0.01, amp);
  } else {
    // Step 1: Gain in 0.01 steps from 0.5 to 1.5
    int gainStepsCoarseN = (int)((gain_coarse_max - gain_coarse_min) / 0.01 / 2);

    tft.setFontScale((enum RA8875tsize)0);
    tft.fillRect(menuX, 280, 250, tft.getFontHeight(), RA8875_BLACK);
    tft.setTextColor(RA8875_YELLOW);
    tft.setCursor(menuX, 280);
    tft.print("  1. Adjusting course gain...");

    TuneCalParameter(-gainStepsCoarseN, gainStepsCoarseN + 1, 0.01, amp);

    // Step 2: phase changes in 0.01 steps from -0.2 to 0.2. Find the minimum.
    int phaseStepsCoarseN = (int)((phase_coarse_max - phase_coarse_min) / 0.01 / 2);

    tft.setFontScale((enum RA8875tsize)0);
    tft.fillRect(menuX, 280, 250, tft.getFontHeight(), RA8875_BLACK);
    tft.setTextColor(RA8875_YELLOW);
    tft.setCursor(menuX, 280);
    tft.print("  2. Adjusting course phase...");

    TuneCalParameter(-phaseStepsCoarseN, phaseStepsCoarseN + 1, 0.01, phase);
  }

  // Step 3: Gain in 0.01 steps from 4 steps below previous minimum to 4 steps above
  tft.setFontScale((enum RA8875tsize)0);
  tft.fillRect(menuX, 280, 250, tft.getFontHeight(), RA8875_BLACK);
  tft.setTextColor(RA8875_YELLOW);
  tft.setCursor(menuX, 280);
  tft.print("  3. Adjusting gain...");

  TuneCalParameter(-gain_coarse_step2_N, gain_coarse_step2_N + 1, 0.01, amp);

  // Step 4: phase in 0.01 steps from 4 steps below previous minimum to 4 steps above
  tft.setFontScale((enum RA8875tsize)0);
  tft.fillRect(menuX, 280, 250, tft.getFontHeight(), RA8875_BLACK);
  tft.setTextColor(RA8875_YELLOW);
  tft.setCursor(menuX, 280);
  tft.print("  4. Adjusting phase...");

  TuneCalParameter(-phase_coarse_step2_N, phase_coarse_step2_N + 1, 0.01, phase);

  // Step 5: gain in 0.001 steps 10 steps below to 10 steps above
  tft.setFontScale((enum RA8875tsize)0);
  tft.fillRect(menuX, 280, 250, tft.getFontHeight(), RA8875_BLACK);
  tft.setTextColor(RA8875_YELLOW);
  tft.setCursor(menuX, 280);
  tft.print("  5. Adjusting fine gain...");

  TuneCalParameter(-gain_fine_N, gain_fine_N + 1, 0.001, amp);

  // Step 6: phase in 0.001 steps 10 steps below to 10 steps above
  tft.setFontScale((enum RA8875tsize)0);
  tft.fillRect(menuX, 280, 250, tft.getFontHeight(), RA8875_BLACK);
  tft.setTextColor(RA8875_YELLOW);
  tft.setCursor(menuX, 280);
  tft.print("  6. Adjusting fine phase...");

  TuneCalParameter(-phase_fine_N, phase_fine_N + 1, 0.001, phase);

  tft.setFontScale((enum RA8875tsize)0);
  tft.fillRect(menuX, 280, 250, 2 * tft.getFontHeight(), RA8875_BLACK);
  tft.setTextColor(RA8875_WHITE);
  tft.setCursor(menuX, 280);
  tft.print("  Auto Cal Done");
}

FLASHMEM void AutoCal() {
  if(transmitCal) {
    // transmit calibration
    AutoTune(&IQXAmpCorrectionFactor[currentBand], &IQXPhaseCorrectionFactor[currentBand],
              GAIN_COARSE_MAX, GAIN_COARSE_MIN,
              PHASE_COARSE_MAX, PHASE_COARSE_MIN,
              GAIN_COARSE_STEP2_N, PHASE_COARSE_STEP2_N,
              GAIN_FINE_N, PHASE_FINE_N, false);
  } else {
    // receive calibration
    AutoTune(&IQAmpCorrectionFactor[currentBand], &IQPhaseCorrectionFactor[currentBand],
              GAIN_COARSE_MAX, GAIN_COARSE_MIN,
              PHASE_COARSE_MAX, PHASE_COARSE_MIN,
              GAIN_COARSE_STEP2_N, PHASE_COARSE_STEP2_N,
              GAIN_FINE_N, PHASE_FINE_N, false);
  }
  UpdateIQDisplay();
}


//-------------------------------------------------------------------------------------------------------------
// Frequency Calibration
//-------------------------------------------------------------------------------------------------------------



//-------------------------------------------------------------------------------------------------------------
// IQ Calibration
//-------------------------------------------------------------------------------------------------------------

/*****
  Purpose: Combined input/output to calibrate the transmit or receive IQ signals
 *****/
FLASHMEM void CalibrateIQ(bool calType) {
  int calFlag = 1; // 1 = do calibration, 0 = done
  int val, bandCalBand;
  unsigned long prevMillis;

  transmitCal = calType;

  // set up for calibration
  if(transmitCal) {
    CalibratePreamble(2, CALIBRATE_TRANSMIT_STATE, CALIBRATE_TRANSMIT_STATE);
  } else {
    //CalibratePreamble(1, CW_TRANSMIT_STRAIGHT_STATE, CALIBRATE_RECEIVE_STATE);
    CalibratePreamble(1, CALIBRATE_TRANSMIT_STATE, CALIBRATE_TRANSMIT_STATE);
  }

  ShowCalibrationDisplay();

  // show manual plot value increment
  tft.setFontScale((enum RA8875tsize)0);
  tft.fillRect(menuX + 20 * tft.getFontWidth(), iqCorIncY - 15, 50, tft.getFontHeight(), RA8875_BLACK);
  tft.setTextColor(RA8875_GREEN);
  tft.setCursor(menuX + 20 * tft.getFontWidth(), iqCorIncY - 15);
  tft.print(plotValueInc ? 1.0 : 0.1, 1);

  signalStrengthSource = 0; // use manual signal strength source

  // show signal source
  tft.setFontScale((enum RA8875tsize)0);
  tft.fillRect(menuX + 20 * tft.getFontWidth(), iqCorIncY + 15, 50, tft.getFontHeight(), RA8875_BLACK);
  tft.setTextColor(RA8875_GREEN);
  tft.setCursor(menuX + 20 * tft.getFontWidth(), iqCorIncY + 15);
  tft.print(signalStrengthSources[signalStrengthSource]);

  signalStrengthReceived = false;

  prevMillis = millis();

  // calibration loop
  while(true) {
    if(calFlag == 0) {
      // calibration has finished
      // clean up and exit
      if(transmitCal) {
        CalibratePost(2);
      } else {
        CalibratePost(1);
      }
      break;
    }

    ShowTransmitReceiveStatus();
    AdjustCalFactors();

    // *** TODO: consider manual cal ***
    GetSignalStrength(&signalStrength, 1, false);
    UpdateCalDisplayData();

    // check for and process button input
    val = ReadSelectedPushButton();
    if(val != BOGUS_PIN_READ) {
      val = ProcessButtonPress(val);
    }

    switch(val) {
      case MENU_OPTION_SELECT: // 0
        // save and exit
        //EEPROMWrite();
        calFlag = 0;
        break;

      case BAND_UP: // 2
        ChangeBand(1);
        centerFreq = bands[currentBand].calFreq;
        SetTxRxFreq(centerFreq);
        //si5351.set_freq((centerFreq + calFreqOffset) * SI5351_FREQ_MULT, SI5351_CLK2);
        UpdateIQDisplay();
        break;

      case BAND_DN: // 3
        ChangeBand(-1);
        centerFreq = bands[currentBand].calFreq;
        SetTxRxFreq(centerFreq);
        //si5351.set_freq((centerFreq + calFreqOffset) * SI5351_FREQ_MULT, SI5351_CLK2);
        UpdateIQDisplay();
        break;

      case SET_MODE: // 8
        // experimental stuff
        // request signal strength
        if(!signalStrengthReceived) {
          SendSignalStrengthRequest();
        }

        /*
        // toggle calibration relay
        calRelayOn = !calRelayOn;
        if(calRelayOn) {
          priorInAtten = inAtten;
          priorOutAtten = outAtten;
          inAtten = 51; // gives about same level as normal transmit w/o CAL relay on
          outAtten = 51;
          Serial.print(inAtten); Serial.print(", "); Serial.print(outAtten); Serial.print(", "); Serial.print(currentRF_InAtten); Serial.print(", "); Serial.println(currentRF_OutAtten);
          SetRF_InAtten(inAtten);
          SetRF_OutAtten(outAtten);
          digitalWrite(RF_CAL_RELAY, ON);

        } else {
          digitalWrite(RF_CAL_RELAY, OFF);
          inAtten = priorInAtten;
          outAtten = priorOutAtten;
          SetRF_InAtten(inAtten);
          SetRF_OutAtten(outAtten);
        }
        PrintAtten();
        */
        break;

      case NOISE_REDUCTION:  // 9
        // plot sginal level value
        PlotIQGainValue(plotValue);
        break;

      case NOTCH_FILTER: // 10
        // toggle image level change, true = 1.0, false = 0.1
        plotValueInc = !plotValueInc;

        tft.setFontScale((enum RA8875tsize)0);
        tft.fillRect(menuX + 20 * tft.getFontWidth(), iqCorIncY - 15, 50, tft.getFontHeight(), RA8875_BLACK);
        tft.setTextColor(RA8875_GREEN);
        tft.setCursor(menuX + 20 * tft.getFontWidth(), iqCorIncY - 15);
        tft.print(plotValueInc ? 1.0 : 0.1, 1);
        break;

      case NOISE_FLOOR: // 11
        // toggle IQ calibration increment
        iqIncrementIndex++;
        if(iqIncrementIndex >= 3) iqIncrementIndex = 0;
        iqCorInc = iqIncrementValues[iqIncrementIndex];

        DisplayIQAdjustIncrement(20);
        break;

      case FINE_TUNE_INCREMENT: // 12
        // toggle signal strength source
        signalStrengthSource++;
        if(signalStrengthSource > 2) signalStrengthSource = 0;

        // show signal source
        tft.setFontScale((enum RA8875tsize)0);
        tft.fillRect(menuX + 20 * tft.getFontWidth(), iqCorIncY + 15, 50, tft.getFontHeight(), RA8875_BLACK);
        tft.setTextColor(RA8875_GREEN);
        tft.setCursor(menuX + 20 * tft.getFontWidth(), iqCorIncY + 15);
        tft.print(signalStrengthSources[signalStrengthSource]);

        tft.setFontScale((enum RA8875tsize)1);
        tft.setTextColor(RA8875_WHITE);
        if(signalStrengthSource == 0) {
          // manual signal strength
          tft.setCursor(menuX, 440);
          tft.print("Plot Val");
          //tft.fillRect(680, 440, 150, tft.getFontHeight(), RA8875_BLACK);
          PrintAtten();
        } else {
          // set up signal strength source
          SetupSignalStrengthSource(signalStrengthSource);

          tft.fillRect(menuX, 400, 150, tft.getFontHeight(), RA8875_BLACK);
          tft.fillRect(menuX, 440, 150, tft.getFontHeight(), RA8875_BLACK);
          tft.setCursor(menuX, 400);
          tft.print("Sig Sup");
          tft.setCursor(menuX, 440);
          tft.print("Max Sup");

          UpdateCalDisplayData();
        }
        break;

      case DECODER_TOGGLE: // 13
        // auto calibrate all bands, starting with the first band

        // setup display and serial tables
        tft.setFontScale((enum RA8875tsize)0);
        tft.fillRect(menuX, iqCorIncY - 30, 250, 14 * tft.getFontHeight(), RA8875_BLACK);
        tft.setTextColor(RA8875_WHITE);
        tft.setCursor(menuX, 10);
        tft.print("All Bands Auto Cal Factors");
        tft.setCursor(menuX, 25);
        //         80M   1.003  0.000
        tft.print("Band  Gain   Phase");

        Serial.println("All Bands Auto Transmit IQ Calibration Factors");
        Serial.println("Band\tGain\tPhase");

        // save current band and set to 80m band here and on external T41
        // *** this code assumes external T41 starts on 40m band ***
        bandCalBand = currentBand;
        ChangeBand(BAND_80M - currentBand);
        SendSetBandChange(-1);

        // cycle through bands doing auto cal
        for(int i = BAND_80M; i < NUMBER_OF_BANDS; i++) {
          // clear previous plot
          DrawIQGainPlot();

          if(bands[currentBand].calFreq > 0) {
            centerFreq = bands[currentBand].calFreq;
            SetTxRxFreq(centerFreq);
            //si5351.set_freq((centerFreq + calFreqOffset) * SI5351_FREQ_MULT, SI5351_CLK2); // v12
            UpdateIQDisplay();

            // set up signal strength source
            SetupSignalStrengthSource(signalStrengthSource);

            // auto calibrate this band
            AutoCal();

            // print factors to display and serial
            tft.setFontScale((enum RA8875tsize)0);
            tft.setTextColor(RA8875_WHITE);
            tft.setCursor(menuX, 10 + 15 * (i + 2));
            tft.print(bands[currentBand].name); tft.print("   "); tft.print(IQXAmpCorrectionFactor[currentBand], 3); tft.print("  "); tft.println(IQXPhaseCorrectionFactor[currentBand], 3);

            Serial.print(bands[currentBand].name); Serial.print("\t"); Serial.print(IQXAmpCorrectionFactor[currentBand], 3); Serial.print("\t"); Serial.println(IQXPhaseCorrectionFactor[currentBand], 3);
          }

          ChangeBand(1);
          SendSetBandChange(1);
        }

        Serial.println();

        // return to original band
        ChangeBand(bandCalBand - currentBand);
        centerFreq = bands[currentBand].calFreq;
        SetTxRxFreq(centerFreq);
        //si5351.set_freq((centerFreq + calFreqOffset) * SI5351_FREQ_MULT, SI5351_CLK2); // v12
        UpdateIQDisplay();
        break;

      case MAIN_TUNE_INCREMENT: // 14
        // auto calibrate current band

        // erase menu
        tft.setFontScale((enum RA8875tsize)0);
        tft.fillRect(menuX, iqCorIncY - 30, 250, 14 * tft.getFontHeight(), RA8875_BLACK);

        // set up signal strength source
        SetupSignalStrengthSource(signalStrengthSource);

        // begin auto calibration for current band
        AutoCal();
        break;

      case BEARING: // 17
        // cancel calibration
        if(transmitCal) {
          IQXAmpCorrectionFactor[currentBand] = userIQAmpFactor;
          IQXPhaseCorrectionFactor[currentBand] = userIQPhaseFactor;
        } else {
          IQAmpCorrectionFactor[currentBand] = userIQAmpFactor;
          IQPhaseCorrectionFactor[currentBand] = userIQPhaseFactor;
        }
        calFlag = 0;
        break;

      default:
        break;
    }
  }
}
