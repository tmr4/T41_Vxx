// v12 specific calibration file

#include <Chrono.h>                // https://github.com/SofaPirate/Chrono/
#include <LinearRegression.h>      // https://github.com/cubiwan/Regressino/
#include <Linear2DRegression.hpp>  // https://github.com/nkaaf/Arduino-Regression
#include <Timer.h>

#include "..\AudioConfig.h"
#include "..\Button.h"
#include "..\ButtonProc.h"
#include "..\debugSerial.h"
#include "..\Demod.h"
#include "..\Display.h"
#include "..\EEPROM.h"
#include "..\Encoders.h"
#include "..\Exciter.h"
#include "..\Filter.h"
#include "..\FIR.h"
#include "..\gwv.h"
#include "..\hardware.h"
#include "displayRA8875\InfoBox.h"
#include "..\Menu.h"
#include "..\MenuProc.h"
#include "..\Process.h"
#include "RF_Control.h"
#include "..\Tune.h"
#include "..\t41Control.h"
#include "..\Utility.h"

// for v12 only

//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

#define RECIEVE_CAL_START_ATTEN 60.0

// *** TODO: new defines help identify what calibration function is selected
//           but provide no feedback on what switch to press.  Perhaps better
//           just to use switch defines. ***
#define CAL_TOGGLE_OUTPUT 12
#define CAL_AUTOCAL 13
#define CAL_DIRECTIONS 14
#define CAL_TOGGLE_ATTENUATOR 15
#define CAL_CHANGE_TYPE 16
#define CAL_CHANGE_INC 11

#define GAIN_COARSE_MAX 1.2
#define GAIN_COARSE_MIN 0.8
#define PHASE_COARSE_MAX 0.2
#define PHASE_COARSE_MIN -0.2

#define GAIN_COARSE_STEP2_N 10
#define PHASE_COARSE_STEP2_N 10
#define GAIN_FINE_N 5
#define PHASE_FINE_N 5

//#define SIG_STRENGTH_MAX 20
#define SIG_STRENGTH_MAX 8

// Add define variables for drive current and load capacitance
#define SI5351_LOAD_CAPACITANCE SI5351_CRYSTAL_LOAD_8PF
#define SI5351_DRIVE_CURRENT SI5351_DRIVE_2MA
extern Si5351 si5351;

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
bool outputAttenAdjustActiveFlag = true;  // RF attenuator adjust active: true: output, false: input
int outAtten = RECIEVE_CAL_START_ATTEN;
int inAtten = RECIEVE_CAL_START_ATTEN;

// frequency calibration
Chrono calChrono;
Chrono calChrono2;

// receive calibration
float aveAdjdB2;
float adjdB2;
float adjdBIQ;
float adjdB;
int recCalNFAdjust = 0;
long calFreqOffset = 0; // manual adjustment to calibration frequency

// transmit calibration
float plotValue = 0;
bool plotValueInc = true; // true = 1.0, false = 0.1
int signalStrengthSource = 0; // signal strength source: 0 = manual user entry, 1 = external via CAT SM command, 2 = internal loopback
const char *signalStrengthSources[3] =  {"man", "ext", "loop"};

// two tone variables
int numTwoToneCycles1 = 8;
int numTwoToneCycles2 = 20;

int16_t pixelold[SPECTRUM_RES], pixelCurrent[SPECTRUM_RES];
int currentNF = 0;

extern bool FFTupdated;

//-------------------------------------------------------------------------------------------------------------
// Forwards
//-------------------------------------------------------------------------------------------------------------

void CalcZoomFreqSpec(uint32_t blockSize); // needed for ProcessTransmitCalIQData

//-------------------------------------------------------------------------------------------------------------
// Code
//------------------------------------------------------------------------------------------------------------------

FLASHMEM void ButtonZoom() {
  SetZoom(spectrumZoom+1);
}

#define CAL_POWER_LEVEL_W 10
FLASHMEM int getPowerLevelAdjustmentDB() {
  return (int)round(- 20*log10f_fast((float)transmitPowerLevel / (float)CAL_POWER_LEVEL_W));
}

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
  userCenterFreq = t41.CenterFreq;
  userNCOFreq = t41.NCOFreq;
  userRadioState = radioState;
  userMode = radioMode;
  userDemodMode = currentDemodMode;
  userZoomIndex = spectrumZoom;

  // calibration specific configuration
  switch(calType) {
    case 0: // frequency cal
      userFilterLowCut = t41.FilterLoCut;
      userFilterHiCut = t41.FilterHiCut;

      t41.FilterHiCut = 1000;
      t41.FilterLoCut = -1000;
      currentDemodMode = DEMOD_SAM;
      CalcAudioFilters();

      spectrumZoom = 0; // prevents call to CalcZoomFreqSpec in Process.cpp
      break;

    case 1: // receive IQ cal
      userBand = currentBand;
      userIQAmpFactor = IQAmpCorrectionFactor[currentBand];
      userIQPhaseFactor = IQPhaseCorrectionFactor[currentBand];
      userScale = currentScale;

      currentScale = 1; // set vertical scale to 10 dB during calibration
      displayState = DISPLAY_CALIBRATION;
      spectrumZoom = 0; // prevents call to CalcZoomFreqSpec in Process.cpp

      SetRF_InAtten(RECIEVE_CAL_START_ATTEN);
      SetRF_OutAtten(RECIEVE_CAL_START_ATTEN);

      // set clock 2 to calibration frequency
      si5351.output_enable(SI5351_CLK2, 1);
      si5351.set_freq((t41.CenterFreq + calFreqOffset) * SI5351_FREQ_MULT, SI5351_CLK2);

      // set RF board configuration:
      //   - transmit mode relay to CW
      //   - CW signal on
      //   - calibration relay on
      digitalWrite(RF_XMIT_RELAY, XMIT_CW);
      digitalWrite(RF_CW_SIGNAL, ON);
      digitalWrite(RF_CAL_RELAY, ON);
      break;

    case 2: // transmit IQ cal
      //sgtl5000_1.micGain(currentMicGain);
      //setBPFPath(BPF_IN_TX_PATH);
      //xrState = TRANSMIT_STATE;

      // set RF board configuration:
      //   - transmit mode relay to SSB
      // in case we come here from CW mode *** TODO: verify we get back to XMIT_CW if appropriate ***
      digitalWrite(RF_XMIT_RELAY, XMIT_SSB);


      displayState = DISPLAY_CALIBRATION;
      spectrumZoom = 2;
      //spectrumZoom = 3;
      inAtten = 0;
      outAtten = 10;
      SetRF_InAtten(inAtten);
      SetRF_OutAtten(outAtten);

      userIQAmpFactor = IQXAmpCorrectionFactor[currentBand];
      userIQPhaseFactor = IQXPhaseCorrectionFactor[currentBand];

      //outAtten = 10;
      //inAtten = currentRF_InAtten;
      //sgtl5000_1.micGain(10);
      //SetRF_OutAtten(outAtten);
      ////SetRF_InAtten(inAtten);
      //SetRF_InAtten(63);
      //PrintAtten();
      break;

    case 3: // two tone test
      // set RF board configuration:
      //   - transmit mode relay to SSB
      // in case we come here from CW mode *** TODO: verify we get back to XMIT_CW if appropriate ***
      digitalWrite(RF_XMIT_RELAY, XMIT_SSB);

      outAtten = XAttenSSB[currentBand] + getPowerLevelAdjustmentDB();
      if(outAtten > 63) outAtten = 63;
      if(outAtten < 0) outAtten = 0;
      SetRF_OutAtten(outAtten);
      break;

    default:
      break;
  }

  // general calibration configuration
  // set center frequency
  if(calType == 2 || calType == 3) {
    // remove the IF offset
    // *** why? ***
    t41.CenterFreq = t41.CenterFreq - intermediateFreq + t41.NCOFreq;

  } else {
    t41.CenterFreq = t41.TXRXFreq();
  }
  t41.NCOFreq = 0;

  radioState = rState;
  ConfigAudioState(aState);
  SetFreq(t41.CenterFreq);
}

/*****
  Purpose: clean up after calibration

  Parameter List:
    calType - type of calibration we're cleaning up after
 *****/
FLASHMEM void CalibratePost(int calType) {
  // restore radio operating state
  t41.NCOFreq = userNCOFreq;
  t41.CenterFreq = userCenterFreq;
  radioState = userRadioState;
  currentDemodMode = userDemodMode;
  spectrumZoom = userZoomIndex;

  // calibration specific restoration
  switch(calType) {
    case 0: // frequency cal
      currentDemodMode = userDemodMode;
      t41.FilterLoCut = userFilterLowCut;
      t41.FilterHiCut = userFilterHiCut;
      CalcAudioFilters();
      break;

    case 1: // receive cal
      SetRF_InAtten(currentRF_InAtten);
      SetRF_OutAtten(currentRF_OutAtten);
      displayState = DISPLAY_T41;
      if(currentBand != userBand) {
        ChangeBand(userBand - currentBand);
        currentBand = userBand;
      }

      si5351.output_enable(SI5351_CLK2, 0);
      currentScale = userScale;
      digitalWrite(RF_CAL_RELAY, OFF);
      digitalWrite(RF_CW_SIGNAL, OFF);

      if(userMode == SSB_MODE) {
        digitalWrite(RF_XMIT_RELAY, XMIT_SSB);
      }

      // reset frequency spectrum buffers
      SET_VAR(pixelnew, SPECTRUM_BOTTOM);
      SET_VAR(pixelold, SPECTRUM_BOTTOM);
      //CLEAR_VAR(prevFreqSpecBuf);
      InitFFTArrays();
      newSpectrumFlag = 0;
      break;

    case 2: // transmit cal
      displayState = DISPLAY_T41;
      if(userMode == CW_MODE) {
        digitalWrite(RF_XMIT_RELAY, XMIT_CW);
      }
      SetRF_InAtten(currentRF_InAtten);
      SetRF_OutAtten(currentRF_OutAtten);
      break;

    case 3: // two tone test
      SetRF_InAtten(currentRF_InAtten);
      SetRF_OutAtten(currentRF_OutAtten);
      digitalWrite(RF_CAL_RELAY, OFF);
      break;

    default:
      break;
  }

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

FLASHMEM void UpdateIQDisplay(bool receiveFlag = true, bool autoFlag = false) {
  tft.fillRect(680, 320, 150, CHAR_HEIGHT, RA8875_BLACK);
  tft.setFontScale((enum RA8875tsize)1);
  if(autoFlag) {
    tft.setTextColor(RA8875_YELLOW);
  } else {
    tft.setTextColor(RA8875_GREEN);
  }
  tft.setCursor(680, 320);
  if(receiveFlag) {
    tft.print(IQAmpCorrectionFactor[currentBand], 3);
  } else {
    tft.print(IQXAmpCorrectionFactor[currentBand], 3);
  }
  tft.fillRect(680, 360, 150, CHAR_HEIGHT, RA8875_BLACK);
  tft.setCursor(680, 360);
  if(receiveFlag) {
    tft.print(IQPhaseCorrectionFactor[currentBand], 3);
  } else {
    tft.print(IQXPhaseCorrectionFactor[currentBand], 3);
  }
}

FLASHMEM bool AdjustIQFactors(bool receiveFlag = true) {
  bool adjustFlag = false;

  // IQ amp correction factor
  if(menuEncoderMove != 0) {
    if(receiveFlag) {
      IQAmpCorrectionFactor[currentBand] += menuEncoderMove * iqCorInc;
    } else {
      IQXAmpCorrectionFactor[currentBand] += menuEncoderMove * iqCorInc;
    }

    menuEncoderMove = 0;
    adjustFlag = true;
  }

  // IQ phase correction factor
  if(adjustVolEncoder != 0) {
    if(receiveFlag) {
      IQPhaseCorrectionFactor[currentBand] += adjustVolEncoder * iqCorInc;
    } else {
      IQXPhaseCorrectionFactor[currentBand] += adjustVolEncoder * iqCorInc;
    }

    adjustVolEncoder = 0;
    adjustFlag = true;
  }

  if(adjustFlag) {
    UpdateIQDisplay(receiveFlag);
  }
  return adjustFlag;
}

FLASHMEM void PrintAtten() {
  tft.fillRect(menuX, 400, 300, CHAR_HEIGHT, RA8875_BLACK);
  tft.setFontScale((enum RA8875tsize)1);
  tft.setTextColor(RA8875_WHITE);
  tft.setCursor(menuX, 400);
  if(outputAttenAdjustActiveFlag) {
    tft.print("Out Atten");
  } else {
    tft.print("In Atten");
  }
  tft.setTextColor(RA8875_GREEN);
  tft.setCursor(680, 400);
  if(outputAttenAdjustActiveFlag) {
    tft.print(outAtten / 2.0, 1);
  } else {
    tft.print(inAtten / 2.0, 1);
  }
}

FLASHMEM bool AdjustRxTxAtten(bool receiveFlag = true) {
  bool adjustFlag = false;

  if(fineTuneEncoderMove != 0) {
    if(outputAttenAdjustActiveFlag) {
      outAtten += fineTuneEncoderMove;
      if(outAtten > 63) {
        outAtten = 63;
      }
      if(outAtten < 0) {
        outAtten = 0;
      }
      SetRF_OutAtten(outAtten);
    } else {
      inAtten += fineTuneEncoderMove;
      if(inAtten > 63) {
        inAtten = 63;
      }
      if(inAtten < 0) {
        inAtten = 0;
      }
      SetRF_InAtten(inAtten);
    }

    fineTuneEncoderMove = 0;

    PrintAtten();
    adjustFlag = true;
  }

  return adjustFlag;
}

//-------------------------------------------------------------------------------------------------------------
// Frequency Calibration
//-------------------------------------------------------------------------------------------------------------

// print value, limiting decimals based on value magnitude
FLASHMEM void PrintValue(float value, int decimals) {
  int limit = decimals - max(0, trunc(log10f_fast(value)));
  tft.print(value, limit);
}

FLASHMEM void SetClocks(int adder) {
  si5351.init(SI5351_LOAD_CAPACITANCE, Si_5351_crystal, freqCorrectionFactor + adder);

  si5351.drive_strength(SI5351_CLK0, SI5351_DRIVE_CURRENT);
  si5351.drive_strength(SI5351_CLK1, SI5351_DRIVE_CURRENT);
  si5351.drive_strength(SI5351_CLK2, SI5351_DRIVE_CURRENT);
  si5351.set_ms_source(SI5351_CLK0, SI5351_PLLA);
  si5351.set_ms_source(SI5351_CLK1, SI5351_PLLA);
  //si5351.output_enable(SI5351_CLK2, 0);
  SetFreq(t41.CenterFreq, true);

  // allow clocks to stabilize
  delay(10);
}

FLASHMEM void ShowFreqCalcManualMenu(int freqCorInc) {
  tft.setTextColor(RA8875_YELLOW);
  tft.setFontScale((enum RA8875tsize)0);
  tft.setCursor(20, 240);
  tft.print("11 - Freq cal inc");
  tft.setTextColor(RA8875_GREEN);
  tft.setCursor(20 + 20 * tft.getFontWidth(), 240);
  tft.print(freqCorInc);

  tft.setTextColor(RA8875_YELLOW);
  tft.setCursor(20, 255);
  tft.print("14 - Auto");
  tft.setCursor(20, 270);
  tft.print("15 - Time Plot");
  tft.setCursor(20, 285);
  tft.print("17 - Cancel");
  tft.setCursor(20, 300);
  tft.print("Select - Save/Exit");

  tft.setCursor(20, 320);
  //tft.print("For Time Plot only:");
  tft.print("16 - Directions");
  //tft.setCursor(20, 335);
  //tft.setCursor(20, 350);
  //tft.print("Filter - Time Plot Error Scale");
  //tft.setCursor(20, 380);
  //tft.print("12 - Time Plot Duration");
}

FLASHMEM bool FreqAutoPlot(bool startFlag) {
  const int freqAutoLowSet = 500;
  const int freqAutoIncrementSet = 100;
  float freqError;
  bool completeFlag = false;

  static Linear2DRegression *corrFacReg;
  static Linear2DRegression *plotLine;
  static Linear2DRegression *errorMarker;
  static int errorMarkerPos = 0;
  static int freqCalFactorStart = 0;
  static int autoCount = 0;
  static int autoCalOffset = 0;
  static int correctionFactor = freqCorrectionFactor;

  if(startFlag) {
    // start auto frequency calibration
    corrFacReg = new Linear2DRegression();
    plotLine = new Linear2DRegression();
    errorMarker = new Linear2DRegression();

    // reset regressions
    corrFacReg->reset();
    plotLine->reset();
    errorMarker->reset();

    ClearScreen();

    // print menu
    tft.setFontScale((enum RA8875tsize)0);
    tft.setTextColor(RA8875_YELLOW);
    tft.setCursor(20, 255);
    tft.print("Press any button to cancel");

    // create and fill in info block
    tft.drawRect(470, 55, 310, 110, RA8875_GREEN); // info box
    tft.setFontScale((enum RA8875tsize)1);
    tft.setTextColor(RA8875_WHITE);
    tft.setCursor(480, 60);
    tft.print("Cur Factor");
    tft.fillRect(670, 60, 100, CHAR_HEIGHT, RA8875_BLACK);
    tft.setCursor(670, 60);
    tft.print(freqCorrectionFactor);
    tft.setCursor(480, 95);
    tft.print("Freq Error");
    tft.setCursor(480, 130);
    tft.print("New Factor");

    // draw auto plot frame
    tft.drawRect(300, 200, 480, 270, RA8875_GREEN); // plot box
    tft.drawFastHLine(340, 440, 400, RA8875_GREEN);
    tft.drawFastVLine(340, 210, 230, RA8875_GREEN);
    tft.setFontScale((enum RA8875tsize)0);
    tft.setCursor(310, 200);
    tft.print("5");
    tft.setCursor(310, 440);
    tft.print("-5");
    tft.setFontScale((enum RA8875tsize)0);
    tft.setCursor(310, 270);
    tft.print("Hz");
    tft.setCursor(480, 443);
    tft.print("Corr.");
    for(int k = 0; k < 5; k++) {
      tft.drawFastVLine(340 + k * 100, 440, 7, RA8875_GREEN);
      tft.setCursor(330 + k * 100, 448);
      tft.print(-500 + 250 * k);

      tft.drawFastHLine(333, 210 + k * 57.5, 7, RA8875_GREEN);
    }

    // draw horizontal line at 0 point
    tft.drawFastHLine(340, 325, 400, RA8875_CYAN); // 325 = 210 + 2 * 57.5

    // set first auto calc point
    freqCalFactorStart = freqCorrectionFactor;
    autoCount = 0;

    freqCorrectionFactor = freqCalFactorStart - freqAutoLowSet + autoCount * freqAutoIncrementSet;
    autoCalOffset = -freqAutoLowSet + autoCount * freqAutoIncrementSet;
    SetClocks(0);

    calChrono.restart(0); // reset timer
  } else {
    // process IQ data and calc frequency error each loop
    ProcessReceiverData();
    freqError = 0.20000012146 * SAM_carrier_freq_offset;

    // update frequency error and auto plot every 5 seconds
    if(calChrono.elapsed() >= 5000) {
      // update current frequency calibration factor and error
      tft.setFontScale((enum RA8875tsize)1);
      tft.fillRect(670, 60, 100, CHAR_HEIGHT, RA8875_BLACK);
      tft.setTextColor(RA8875_WHITE);
      tft.setCursor(670, 60);
      tft.print(freqCorrectionFactor);
      tft.fillRect(670, 95, 100, CHAR_HEIGHT, RA8875_BLACK);
      tft.setCursor(670, 95);
      PrintValue(freqError, 3);

      // update auto frequency calibration plot
      int plotX, plotY, plotStartY, plotEndY;
      static int plotXOld, plotStartYOld, plotEndYOld;

      tft.writeTo(L2); // plot data is on layer 2

      // erase old calibration error marker and auto plot line
      if(autoCount > 0) {
        if((plotXOld >= 340) && (plotXOld <= 740) && (plotStartYOld >= 210) && (plotStartYOld <= 440) && (plotEndYOld >= 210) && (plotEndYOld <= 440)) {
          tft.drawLine(340, plotStartYOld, plotXOld, plotEndYOld, RA8875_BLACK);
        }
        tft.drawCircle(errorMarkerPos, 325, 5, RA8875_BLACK);
      }

      // add a new point to auto plot line and error marker regressions
      // and calculate their new positions
      plotX = map(autoCalOffset, -freqAutoLowSet, freqAutoLowSet, 340, 740);
      plotY = map((100 * 0.20000012146 * SAM_carrier_freq_offset), -500, 500, 440, 210);
      plotLine->addPoint(plotX, plotY);
      plotStartY = plotLine->calculate(340);
      plotEndY = plotLine->calculate(plotX);

      errorMarker->addPoint(plotY, plotX);
      errorMarkerPos = errorMarker->calculate(325);

      // update auto plot line and error marker
      if((plotX >= 340) && (plotX <= 740) && (plotStartY >= 210) && (plotStartY <= 440) && (plotEndY >= 210) && (plotEndY <= 440)) {
        tft.drawLine(340, plotStartY, plotX, plotEndY, LIGHT_BLUE);
      }

      // limit error marker position to within graph area
      if(errorMarkerPos < 340) {
        errorMarkerPos = 325;
      } else if(errorMarkerPos > 740) {
        errorMarkerPos = 755;
      }
      tft.drawCircle(errorMarkerPos, 325, 5, RA8875_CYAN);

      // save positions to erase on next pass
      plotStartYOld = plotStartY;
      plotEndYOld = plotEndY;
      plotXOld = plotX;

      // draw regression point and calculated correction factor on layer 1
      tft.writeTo(L1);
      if(plotY > 210 && plotY < 440) {
        tft.fillCircle(plotX, plotY, 2, RA8875_YELLOW);
      }

      // update correction factor regression and display
      corrFacReg->addPoint(0.20000012146 * SAM_carrier_freq_offset, freqCorrectionFactor);
      correctionFactor = corrFacReg->calculate(0);

      if(autoCount > 0) {
        tft.fillRect(670, 130, 100, CHAR_HEIGHT, RA8875_BLACK); // erase old value
        tft.setCursor(670, 130);
        tft.print(correctionFactor);
      }

      autoCount++; // increment auto plot counter

      // set up for next plot point
      tft.fillRect(340, 165, 250, CHAR_HEIGHT, RA8875_BLACK);
      tft.setFontScale((enum RA8875tsize)1);
      tft.setCursor(340, 165);
      tft.print("Auto Tune: ");
      tft.print(autoCount);

      freqCorrectionFactor = freqCalFactorStart - freqAutoLowSet + autoCount * freqAutoIncrementSet;
      autoCalOffset = -freqAutoLowSet + autoCount * freqAutoIncrementSet;

      SetClocks(0);

      calChrono.restart(0);
    }

    if(autoCount >= 11) {
      // auto mode complete, clean up
      delete corrFacReg;
      delete plotLine;
      delete errorMarker;

      // signal auto tune is done
      tft.fillRect(340, 165, 250, CHAR_HEIGHT, RA8875_BLACK);
      tft.setFontScale((enum RA8875tsize)1);
      tft.setCursor(340, 165);
      tft.print("Auto tune done");
      delay(3000);
      tft.fillRect(340, 165, 250, CHAR_HEIGHT, RA8875_BLACK);

      freqCorrectionFactor = correctionFactor;

      completeFlag = true;
    }
  }

  return completeFlag;
}

// *** TODO: this could use some commenting ***
FLASHMEM bool FreqTimePlot(bool startFlag, int plotTimeInterval, float plotScaleNumber) {
  bool completeFlag = false;
  float freqError = 0;
  long remainTimeUpdate = 0;
  static int timeIncrement = 0;
  static unsigned long plotElapsedTimeStart = 0;
  int corrPlotXValue4;
  int corrPlotYValue4;
  int corrPlotYValue5;
  int corrPlotYValue6;
  float32_t corrFactorStdDev = 0.0;
  float32_t corrFactorMean;
  float32_t corrFactorAveMean;
  float32_t corrFactorStdDevAve = 0;
  static float32_t *frequencyDiffValue; // [1250];
  static float32_t *frequencyDiffValueTen; // [20];

  if(startFlag) {
    ClearScreen();

    frequencyDiffValue = new float32_t[1250];
    frequencyDiffValueTen = new float32_t[20];

    timeIncrement = 0;
    plotElapsedTimeStart = millis();
    calChrono2.restart(0);
    calChrono.restart(0);

    // draw time plot screen
    tft.setFontScale((enum RA8875tsize)0);
    //tft.fillRect(130, 50, 80, tft.getFontHeight(), RA8875_BLACK);
    tft.setTextColor(RA8875_YELLOW);
    tft.setCursor(130, 50);
    tft.print((float)plotTimeInterval / 3600.);
    //tft.fillRect(299, 200, 500, 279, RA8875_BLACK);
    tft.drawRect(10, 200, 778, 270, RA8875_GREEN);
    tft.fillRect(11, 201, 776, 268, RA8875_BLACK);
    tft.drawFastHLine(50, 450, 700, RA8875_GREEN);
    tft.drawFastVLine(50, 220, 230, RA8875_GREEN);
    tft.setFontScale((enum RA8875tsize)0);
    tft.setTextColor(RA8875_CYAN);
    tft.setCursor(12, 210);
    tft.print(plotScaleNumber, 1);
    //tft.fillRect(12, 440, 30, CHAR_HEIGHT, RA8875_BLACK);
    tft.setCursor(12, 440);
    tft.print(-plotScaleNumber, 1);
    tft.setCursor(12, 330);
    tft.print("  0");
    tft.setCursor(12, 270);
    tft.print(" Hz");
    tft.drawFastHLine(50, 330, 700, RA8875_CYAN);
    tft.setCursor(400, 425);
    tft.print("Time->");
  } else {
    // process IQ data and calc frequency error each loop
    ProcessReceiverData();
    freqError = 0.20000012146 * SAM_carrier_freq_offset;

    // update display
    tft.setTextColor(RA8875_YELLOW);
    tft.setFontScale((enum RA8875tsize)0);
    tft.setCursor(10, 50);
    tft.print("Plot Time hrs.");

    tft.setFontScale((enum RA8875tsize)1);
    tft.setCursor(5, 160);
    tft.setTextColor(LIGHT_BLUE);
    tft.print("Ave Mean");
    tft.setTextColor(RA8875_RED);
    tft.setCursor(5, 125);
    tft.print("Run Mean");
    tft.setTextColor(LIGHT_BLUE);
    tft.setFontScale((enum RA8875tsize)0);
    tft.setCursor(10, 103);
    tft.print("#");
    tft.setCursor(130, 103);
    tft.setTextColor(RA8875_WHITE);
    tft.print("Sec");

    tft.setFontScale((enum RA8875tsize)1);
    tft.setCursor(265, 160);
    tft.print("StdDev");

    tft.setFontScale((enum RA8875tsize)0);
    tft.setTextColor(RA8875_YELLOW);
    tft.setFontScale((enum RA8875tsize)0);
    if(millis() - remainTimeUpdate > 1000) {  // 1 second
      tft.setTextColor(RA8875_WHITE);
      tft.fillRect(287, 50, 180, tft.getFontHeight(), RA8875_BLACK);
      if((float)(plotTimeInterval - timeIncrement * plotTimeInterval / 1200) <= 3600.) {
        tft.setCursor(190, 50);
        tft.print("Remain Time min.");
        tft.setCursor(330, 50);
        tft.print((float)(plotTimeInterval - (float)(timeIncrement * plotTimeInterval / 1200)) / 60, 1);

      } else {
        if((float)(plotTimeInterval - (float)(timeIncrement * plotTimeInterval / 1200)) > 3600) {
          //tft.fillRect(300, 50, 200, CHAR_HEIGHT, RA8875_BLACK);
          tft.setCursor(190, 50);
          tft.print("Remain Time hrs.");
          tft.setCursor(330, 50);
          tft.print((float)(plotTimeInterval - (timeIncrement * plotTimeInterval / 1200)) / 3600, 2);
        }
      }
      remainTimeUpdate = millis();
    }
    if(calChrono2.elapsed() >= (long unsigned int)plotTimeInterval) {  //next point is 1.2 sec, 3.6 sec, 10.8 sec or 36 sec Chrono counts in MS
      tft.setFontScale((enum RA8875tsize)0);
      tft.setTextColor(LIGHT_BLUE);
      tft.setCursor(20, 103);
      tft.fillRect(20, 103, 50, CHAR_HEIGHT, RA8875_BLACK);
      tft.print(timeIncrement);
      tft.setTextColor(RA8875_WHITE);
      tft.setCursor(70, 103);
      tft.fillRect(70, 103, 70, CHAR_HEIGHT, RA8875_BLACK);
      tft.print((millis() - plotElapsedTimeStart) / 1000);

      corrPlotXValue4 = map(timeIncrement, 0, 1200, 60, 760);
      corrPlotYValue4 = map(1000 * ((0.20000012146 * SAM_carrier_freq_offset)), -plotScaleNumber * 1000, plotScaleNumber * 1000, 440, 210);
      if(corrPlotYValue4 >= 400) corrPlotYValue4 = 439;
      if(corrPlotYValue4 <= 210) corrPlotYValue4 = 211;
      tft.fillCircle(corrPlotXValue4, corrPlotYValue4, 2, RA8875_YELLOW);

      frequencyDiffValue[timeIncrement] = 0.20000012146 * SAM_carrier_freq_offset;
      tft.setFontScale((enum RA8875tsize)1);
      arm_mean_f32(frequencyDiffValue, timeIncrement, &corrFactorAveMean);

      tft.setTextColor(LIGHT_BLUE);
      tft.setCursor(150, 160);
      tft.fillRect(150, 160, 95, CHAR_HEIGHT, RA8875_BLACK);
      tft.print(corrFactorAveMean, 3);

      if(timeIncrement >= 20) {  //Running Average reading for plot timencrement = 1.2, 3.6, 10.8 or 36 sec
        for(int i = 0; i < 20; i++) {
          frequencyDiffValueTen[i] = frequencyDiffValue[timeIncrement - (20 - i)];
        }
        arm_std_f32(frequencyDiffValueTen, 20, &corrFactorStdDev);
        arm_mean_f32(frequencyDiffValueTen, 20, &corrFactorMean);
        tft.setCursor(150, 125);
        tft.fillRect(150, 125, 120, CHAR_HEIGHT, RA8875_BLACK);

        tft.setTextColor(RA8875_RED);
        tft.print(corrFactorMean, 3);
        corrPlotXValue4 = map(timeIncrement, 0, 1200, 60, 760);
        corrPlotYValue6 = map(1000 * ((corrFactorMean)), -plotScaleNumber * 1000, plotScaleNumber * 1000, 440, 210);
        corrPlotYValue5 = map(1000 * ((corrFactorAveMean)), -plotScaleNumber * 1000, plotScaleNumber * 1000, 440, 210);
        if(corrPlotYValue5 >= 400) corrPlotYValue5 = 439;
        if(corrPlotYValue5 <= 210) corrPlotYValue5 = 211;
        if(corrPlotYValue6 >= 400) corrPlotYValue6 = 439;
        if(corrPlotYValue6 <= 210) corrPlotYValue6 = 211;
        //corrPlotYValue4 = map((int)1000 * 0.05, -100, 100, 440, 210);
        tft.fillCircle(corrPlotXValue4, corrPlotYValue6, 3, RA8875_RED);
        tft.fillCircle(corrPlotXValue4, corrPlotYValue5, 3, LIGHT_BLUE);
      }
      // Print values to screen
      tft.setTextColor(RA8875_WHITE);
      corrFactorStdDevAve = (corrFactorStdDevAve + corrFactorStdDev) / timeIncrement;
      tft.setCursor(375, 160);
      tft.fillRect(375, 160, 95, CHAR_HEIGHT, RA8875_BLACK);
      tft.print(corrFactorStdDev, 3);
      tft.setCursor(670, 95);
      tft.fillRect(670, 95, 100, CHAR_HEIGHT, RA8875_BLACK);
      PrintValue(freqError, 3);

      timeIncrement++;

      calChrono2.restart(0);
      calChrono.restart(0);

      if(timeIncrement >= 1200) {  //Stop the plot after 1200 points
        completeFlag = true;
        delete[] frequencyDiffValue;
        delete[] frequencyDiffValueTen;
      }
    }
  }

  return completeFlag;
}

/*****
  Purpose: Set up prior to IQ calibrations.
  These things need to be saved here and restored in the prologue function:
  Vertical scale in dB  (set to 10 dB during calibration)
  Zoom, set to 1X in receive and 4X in transmit calibrations.
  Transmitter power, set to 5W during both calibrations.

  Parameter List:
    void
 *****/
FLASHMEM void CalibrateFrequency() {
  int freqCalFlag = 1; // 1 = do calibration, 0 = done
  int freqCalMode = 0; // 0 = manual, 1 = auto, 2 = time plot
  float freqError;
  int val;

  int freqCorIndex = 3; // 1000 to start
  int freqCorrIncrementValues[] = { 1, 10, 100, 1000 };
  int freqCorInc = freqCorrIncrementValues[freqCorIndex];

  // time plot parameters
  int plotTimeInterval = 1200;
  float plotScaleNumber = 0.0;
  int plotScaleIndex = 0;
  float plotScaleValues[] = { 0.2, 1.0, 5.0, 10.0 };
  int plotIntervalIndex = 0;
  long plotIntervalValues[] = { 1200, 3600, 10800, 36000 };

  static float freqCorrectionFactorOld = freqCorrectionFactor;
  static float userFreqCorrectionFactor = freqCorrectionFactor;

  // Save the current operating state to restore later
  // and configure radio state for frequency calibration
  CalibratePreamble(0, SSB_RECEIVE_STATE, SSB_RECEIVE_STATE);

  // setup display for frequency calibration
  tft.writeTo(L2);
  tft.clearMemory();
  tft.writeTo(L1);
  tft.clearMemory();

  ShowFreqCalcManualMenu(freqCorInc);

  // frequency calibration loop
  while(1) {
    if(freqCalFlag == 0) {
      // frequency calibration has finished
      // clean up and exit
      CalibratePost(0);
      break;
    }

    // process frequency calibration mode
    switch(freqCalMode) {
      case 0: // manual mode

        freqCorrectionFactor = (long long)GetEncoderValueLive(-200000.0, 200000.0, freqCorrectionFactor, (float)freqCorInc, (char *)"Freq Cal: ");

        if(freqCorrectionFactor != freqCorrectionFactorOld) {
          SetClocks(0);

          freqCorrectionFactorOld = freqCorrectionFactor;
        } else {
          delay(50); // prevent churn
        }

        ProcessReceiverData();

        freqError = 0.20000012146 * SAM_carrier_freq_offset;

        tft.setFontScale((enum RA8875tsize)1);
        tft.setTextColor(RA8875_WHITE);
        tft.setCursor(257, CHAR_HEIGHT + 1);
        tft.print("Freq Error: ");
        tft.fillRect(440, CHAR_HEIGHT + 1, 285, CHAR_HEIGHT, RA8875_BLACK);
        tft.setCursor(440, CHAR_HEIGHT + 1);
        PrintValue(freqError, 4);
        break;

      case 1: // auto mode
        // process another auto frequency calibration point
        if(FreqAutoPlot(false)) {
          // erase info box and menu
          tft.fillRect(470, 55, 310, 110, RA8875_BLACK); // info box
          tft.fillRect(0, 200, 299, 280, RA8875_BLACK); // menu

          ShowFreqCalcManualMenu(freqCorInc);

          // enter manual mode
          freqCalMode = 0;
        }
        break;

      case 2: // time plot mode
        if(FreqTimePlot(false, plotTimeInterval, plotScaleNumber)) {
          // clear current layer
          tft.clearScreen();

          ShowFreqCalcManualMenu(freqCorInc);

          // enter manual mode
          freqCalMode = 0;
        }
        break;

      default:
        break;
    }

    // check and process menu selection
    val = ReadSelectedPushButton();
    if(val != BOGUS_PIN_READ) {
      val = ProcessButtonPress(val);

      if(freqCalMode == 1 || freqCalMode == 2) {
        // any button press in time plot mode is a cancel
        val = -1;

        if(freqCalMode == 1) {
          // erase info box, auto tune line and menu
          tft.fillRect(470, 55, 310, 110, RA8875_BLACK); // info box
          tft.fillRect(340, 165, 250, CHAR_HEIGHT, RA8875_BLACK); // auto tune line
          tft.fillRect(0, 200, 299, 280, RA8875_BLACK); // menu
        } else {
          // clear current layer
          tft.clearScreen();
        }

        ShowFreqCalcManualMenu(freqCorInc);

        // enter manual mode
        freqCalMode = 0;
      }

      switch(val) {
        case MENU_OPTION_SELECT: // 0
          // Save frequency calibration factor and exit
          EEPROMWrite();
          freqCalFlag = 0;
          break;

        case NOISE_FLOOR: // 11
          // toggle freq calibration increment
          freqCorIndex++;
          if(freqCorIndex >= 4) freqCorIndex = 0;
          freqCorInc = freqCorrIncrementValues[freqCorIndex];

          tft.setFontScale((enum RA8875tsize)0);
          tft.fillRect(20 + 20 * tft.getFontWidth(), 240, 5 * tft.getFontWidth(), tft.getFontHeight(), RA8875_BLACK);
          tft.setTextColor(RA8875_GREEN);
          tft.setCursor(20 + 20 * tft.getFontWidth(), 240);
          tft.print(freqCorInc);
          break;

        case FINE_TUNE_INCREMENT: // 12
          // set time plot time interval
          if(freqCalMode == 2) {
            plotIntervalIndex++;
            if(plotIntervalIndex > 3) plotIntervalIndex = 0;
            plotTimeInterval = plotIntervalValues[plotIntervalIndex];

            tft.setFontScale((enum RA8875tsize)0);

            tft.fillRect(130, 50, 80, tft.getFontHeight(), RA8875_BLACK);
            tft.setTextColor(RA8875_YELLOW);
            tft.setCursor(130, 50);
            tft.print((float)plotTimeInterval / 3600.);
          }
          break;

        case DECODER_TOGGLE: // 13
          // set time plot vertical scale
          if(freqCalMode == 2) {
            plotScaleIndex++;
            if(plotScaleIndex > 3) plotScaleIndex = 0;
            plotScaleNumber = plotScaleValues[plotScaleIndex];
            tft.setFontScale((enum RA8875tsize)0);
            tft.fillRect(12, 210, 30, CHAR_HEIGHT, RA8875_BLACK);
            tft.setCursor(12, 210);

            tft.setTextColor(RA8875_CYAN);
            tft.print(plotScaleNumber, 1);
            tft.fillRect(12, 440, 30, CHAR_HEIGHT, RA8875_BLACK);
            tft.setCursor(12, 440);
            tft.print(-plotScaleNumber, 1);
            tft.setCursor(12, 330);
            tft.print("  0");
            tft.setCursor(12, 270);
            tft.print("Hz");
          }
          break;

        case MAIN_TUNE_INCREMENT: // 14
          // start/cancel auto frequency calibration

          // erase manual mode frequency cal and error lines
          tft.fillRect(257, 1, 300, CHAR_HEIGHT * 2, RA8875_BLACK);

          freqCalMode = 1;
          FreqAutoPlot(true);
          break;

        case RESET_TUNING: // 15
          // start time plot

          // erase manual mode frequency cal and error lines
          tft.fillRect(257, 1, 300, CHAR_HEIGHT * 2, RA8875_BLACK);

          freqCalMode = 2;
          plotScaleIndex = 0;
          plotScaleNumber = plotScaleValues[plotScaleIndex];
          plotIntervalIndex = 0;
          plotTimeInterval = plotIntervalValues[plotIntervalIndex];
          FreqTimePlot(true, plotTimeInterval, plotScaleNumber);
          break;

        case BEARING: // 17
          // cancel frequency calibration

          // restore old frequency correction factor
          freqCorrectionFactor = userFreqCorrectionFactor;

          freqCalFlag = 0;
          break;

        default:
          break;
      }
    }
  }
}

//-------------------------------------------------------------------------------------------------------------
// Receive IQ Calibration
//-------------------------------------------------------------------------------------------------------------

FLASHMEM void AdjustReceiveCalFactors() {
  bool adjustFlag = false;
  bool rxtxFlag;

  adjustFlag = AdjustIQFactors();

  rxtxFlag = AdjustRxTxAtten();

  // adjust noise floor
  ProcessCenterTuneEncoder(READ_CENTERTUNE_ENCODER);
  if(tuneChange != 0) {
    recCalNFAdjust -= tuneChange;
    tuneChange = 0;
  }

  if(adjustFlag || rxtxFlag) {
    tft.fillRect(680, 440, 150, CHAR_HEIGHT, RA8875_BLACK);
    tft.setTextColor(RA8875_WHITE);
    tft.setCursor(680, 440);
    tft.print(aveAdjdB2, 1);
  }
}

/*****
  Purpose:  Plot Receive Calibration Spectrum
            This function plots a partial spectrum during calibration only.
            This is intended to increase the efficiency and therefore the responsiveness of the calibration encoder.
            This function is called by ShowSpectrum2() in two for-loops.  One for-loop is for the refenence signal,
            and the other for-loop is for the undesired sideband.
  Parameter list:
    int x1, where x1 is the FFT bin.
    cal_bins[2] locations of the desired and undesired signals
    capture_bins width of the bins used to display the signals
  Return value;
    float returns the adjusted value in dB
*****/

static int oldNF = recCalNFAdjust;

FLASHMEM float PlotCalSpectrum(int x1, int cal_bins[2], int capture_bins) {
  int16_t y_new, y1_new, y_old, y_old2;
  int16_t adjAmplitude = 0;
  int16_t refAmplitude = 0;
  uint32_t index_of_max;
  bool updateSpectrumData;

  adjdB = 0.0;

  // refresh spectrum at appropriate point
  if(x1 == (cal_bins[0] - capture_bins)) {
    updateSpectrumData = true;
  } else {
    updateSpectrumData = false;
  }

  //-------------------------------------------------------
  // This block of code, which calculates the latest FFT and finds the maxima of the tone
  // and its image product, does not technically need to be run every time we plot a pixel
  // on the screen. However, according to the comments below this is needed to eliminate
  // conflicts.
  //AdjustReceiveCalFactors();
  ProcessReceiverData(updateSpectrumData);  // Call the Audio process from within the display routine to eliminate conflicts with drawing the spectrum and waterfall displays

  // Find the maximums of the desired and undesired signals.
  arm_max_q15(&pixelnew[(cal_bins[0] - capture_bins)], capture_bins * 2, &refAmplitude, &index_of_max);
  arm_max_q15(&pixelnew[(cal_bins[1] - capture_bins)], capture_bins * 2, &adjAmplitude, &index_of_max);

  y_new = spectrumNoiseFloor + recCalNFAdjust - pixelnew[x1];
  y1_new = spectrumNoiseFloor + recCalNFAdjust - pixelnew[x1 - 1];
  y_old = spectrumNoiseFloor + oldNF - pixelold[x1];
  y_old2 = spectrumNoiseFloor + oldNF - pixelold[x1 - 1];

  if(y_new > SPECTRUM_BOTTOM) y_new = SPECTRUM_BOTTOM;
  if(y_old > SPECTRUM_BOTTOM) y_old = SPECTRUM_BOTTOM;
  if(y_old2 > SPECTRUM_BOTTOM) y_old2 = SPECTRUM_BOTTOM;
  if(y1_new > SPECTRUM_BOTTOM) y1_new = SPECTRUM_BOTTOM;

  if(y_new < SPECTRUM_TOP_Y) y_new = SPECTRUM_TOP_Y;
  if(y_old < SPECTRUM_TOP_Y) y_old = SPECTRUM_TOP_Y;
  if(y_old2 < SPECTRUM_TOP_Y) y_old2 = SPECTRUM_TOP_Y;
  if(y1_new < SPECTRUM_TOP_Y) y1_new = SPECTRUM_TOP_Y;

  // Erase the old spectrum and draw the new spectrum.
  tft.drawLine(x1, y_old2, x1, y_old, RA8875_BLACK);   // Erase old...
  tft.drawLine(x1, y1_new, x1, y_new, RA8875_YELLOW);  // Draw new
  pixelCurrent[x1] = pixelnew[x1]; // This is the actual "old" spectrum!
                                   // This is required due to CW interrupts.  Copied to pixelold by the FFT function.

  adjdB = ((float)adjAmplitude - (float)refAmplitude) / 1.95;
  tft.writeTo(L2);
  tft.setFontScale((enum RA8875tsize)0);

  tft.setTextColor(RA8875_WHITE);
  tft.setCursor(cal_bins[1] - capture_bins + 20, 144);  // 350, 125
  tft.print("IQ Image");
  tft.setCursor(cal_bins[0] - capture_bins - 88, 144);  // 350, 125
  tft.print("Ref Level");
  tft.drawFastHLine(cal_bins[0] - capture_bins - 15, 152, 15, RA8875_GREEN);
  tft.drawFastHLine(cal_bins[0] - capture_bins + 22, 152, 15, RA8875_GREEN);
  tft.fillRect(cal_bins[0] - capture_bins, SPECTRUM_TOP_Y, 2 * capture_bins, SPECTRUM_HEIGHT, RA8875_BLUE);  // SPECTRUM_TOP_Y = 100
  tft.fillRect(cal_bins[1] - capture_bins, SPECTRUM_TOP_Y, 2 * capture_bins, SPECTRUM_HEIGHT, DARK_RED);     // h = SPECTRUM_HEIGHT + 3

  tft.writeTo(L1);
  return adjdB;
}

/*****
  Purpose: Show Spectrum display modified for IQ calibration.
           This is similar to the function used for normal reception, however, it has
           been simplified and streamlined for calibration.


  Return value;
    void
*****/
FLASHMEM float ShowSpectrum2() {
  int calTypeFlag = 0; // RX state *** TODO: not really used anymore; clean up to present state ***
  int cal_bins[2] = { 0, 0 };
  int capture_bins;  // Sets the number of bins to scan for signal peak.

  adjdB = 0.0;

  pixelnew[0] = 0;
  pixelnew[1] = 0;
  pixelold[0] = 0;
  pixelold[1] = 0;

  //  This is the "spectra scanning" for loop.  During calibration, only small areas of the spectrum need to be examined.
  //  If the entire 512 wide spectrum is used, the calibration loop will be slow and unresponsive.
  //  The scanning areas are determined by receive versus transmit calibration, and LSB or USB.  Thus there are 4 different scanning zones.
  //  All calibrations use a 0 dB reference signal and an "undesired sideband" signal which is to be minimized relative to the reference.
  //  Thus there is a target "bin" for the reference signal and another "bin" for the undesired sideband.
  //  The target bin locations are used by the for-loop to sweep a small range in the FFT.  A maximum finding function finds the peak signal strength.

  /*************************************
  ProcessIQData2 performs an N-point (SPECTRUM_RES = 512) FFT on the data in audioBufferL and
  audioBufferR when they fill up. The data in audioBufferL and audioBufferR are sampled at
  192000 Hz. The length of the audioBufferL and audioBufferR buffers is 2048
  = 128*16 = 2048, of which the FFT only uses the first 512 points.
    N_BLOCKS = FFT_LENGTH / 2 / 128 * (uint32_t)DF
             = 512 / 2 / 128 * 8
             = 16
  Therefore the bin width of each FFT bin is SAMPLE_RATE / FFT_LEN = 192000 / 512 = 375 Hz.
  The frequency of the middle bin is t41.CenterFreq + intermediateFreq and our spectrum spans
  (t41.CenterFreq + intermediateFreq - SAMPLE_RATE/2) to (t41.CenterFreq + intermediateFreq + SAMPLE_RATE/2).

  So the equation for bin number n given frequency Clk2SetFreq is:
    n = (Clk2SetFreq - Clk1SetFreq)/375 + 256
      = (Clk2SetFreq - (t41.CenterFreq + intermediateFreq))/375 + 256

  In receive cal mode, we set Clk2SetFreq to t41.CenterFreq + 2*intermediateFreq
  So we expect the desired tone to appear in bin
    n_tone = intermediateFreq/375 + 256
  while the undesired image product will be at
    n_image= -intermediateFreq/375 + 256

  Which are, given intermediateFreq = 48000:
    n_tone = 384
    n_image= 128
  *********************************************/

  if(calTypeFlag == 0) {
    capture_bins = 10;
    cal_bins[0] = 128 + calFreqOffset / 375;
    cal_bins[1] = 384 - calFreqOffset / 375;
  }  // Receive calibration

  /******************************
   * The same LO clock is used for transmit and receive, so the bin tone and image are
   * found symmetric around the center of the FFT. This offset is 750 Hz (see GenSineToneBuffers()
   * in Utility.cpp). We have zoom of x16, so the bin size is 375/16 = 23.4 Hz. So the
   * bin numbers are 256 + 750/(375/16) = 256+32 = 288 and 256-32 = 224
   ******************************/
  if(calTypeFlag == 1 && currentDemodMode == DEMOD_LSB) {
    capture_bins = 10;  // scans 2*capture_bins
    cal_bins[0] = 257 - 32;
    cal_bins[1] = 257 + 32;
  }  // Transmit calibration, LSB.
  if(calTypeFlag == 1 && currentDemodMode == DEMOD_USB) {
    capture_bins = 10;  // scans 2*capture_bins
    cal_bins[0] = 257 + 32;
    cal_bins[1] = 257 - 32;
  }  // Transmit calibration, USB.

  AdjustReceiveCalFactors();
  //Serial.print("before: "); Serial.print(recCalNFAdjust); Serial.print(", "); Serial.println(oldNF);
  //  There are 2 for-loops, one for the reference signal and another for the undesired sideband.
  for(int i = cal_bins[0] - capture_bins; i < cal_bins[0] + capture_bins; i++) {
    adjdBIQ = PlotCalSpectrum(i, cal_bins, capture_bins);
  }
  //Serial.print("mid: "); Serial.print(recCalNFAdjust); Serial.print(", "); Serial.println(oldNF);
  for(int i = cal_bins[1] - capture_bins; i < cal_bins[1] + capture_bins; i++) {
    adjdBIQ = PlotCalSpectrum(i, cal_bins, capture_bins);
  }
  //Serial.print("after: "); Serial.print(recCalNFAdjust); Serial.print(", "); Serial.println(oldNF);
  oldNF = recCalNFAdjust;

  // Finish up

  tft.setFontScale((enum RA8875tsize)1);
  tft.fillRect(680, 440, 100, tft.getFontHeight(), RA8875_BLACK);
  tft.setCursor(680, 440);  // 350, 125
  adjdB2 = adjdBIQ;
  aveAdjdB2 = 0.9 * aveAdjdB2 + 0.1 * adjdB2;
  tft.print(aveAdjdB2, 1);
  delay(10);

  return adjdBIQ;
}

/*****
  Purpose: Combined input/ output for the purpose of calibrating the receive IQ

   Parameter List:
      void

   Return value:
      void
 *****/
FLASHMEM void tuneCalParameterRec(int indexStart, int indexEnd, float increment, float *IQCorrectionFactor, char prompt[]) {
  int16_t adjAmplitude = 0;
  int16_t refAmplitude = 0;
  uint32_t index_of_max;

  float adjMin = 100;
  int adjMinIndex = 0;
  int cal_bins[2] = { 0, 0 };
  int capture_bins;
  capture_bins = 10;
  cal_bins[0] = 128 + calFreqOffset / 375;
  cal_bins[1] = 384 - calFreqOffset / 375;
  float correctionFactor = *IQCorrectionFactor;
  for(int i = indexStart; i < indexEnd; i++) {
    *IQCorrectionFactor = correctionFactor + i * increment;

    //tft.setFontScale((enum RA8875tsize)0);
    //tft.fillRect(650, 90, 50, tft.getFontHeight(), RA8875_BLACK);
    //tft.setTextColor(RA8875_YELLOW);
    //tft.setCursor(650, 90);
    //tft.print(i * increment, 3);
    FFTupdated = false;
    //int XmitCalDirections = 0;
    while(!FFTupdated) {
      //===============
      ShowSpectrum2();
      arm_max_q15(&pixelnew[(cal_bins[0] - capture_bins)], capture_bins * 2, &refAmplitude, &index_of_max);
      arm_max_q15(&pixelnew[(cal_bins[1] - capture_bins)], capture_bins * 2, &adjAmplitude, &index_of_max);
      adjdB = ((float)adjAmplitude - (float)refAmplitude) / 1.95;
      //==============
    }

    ShowSpectrum2();
    arm_max_q15(&pixelnew[(cal_bins[0] - capture_bins)], capture_bins * 2, &refAmplitude, &index_of_max);
    arm_max_q15(&pixelnew[(cal_bins[1] - capture_bins)], capture_bins * 2, &adjAmplitude, &index_of_max);
    adjdB = ((float)adjAmplitude - (float)refAmplitude) / 1.95;

    if(adjdB < adjMin) {
      adjMin = adjdB;
      adjMinIndex = i;
    }
    tft.fillRect(145, 150, 230, CHAR_HEIGHT, RA8875_BLACK);
    tft.setFontScale((enum RA8875tsize)1);
    tft.setTextColor(RA8875_WHITE);
    tft.setCursor(145, 150);
    tft.print("Auto Cal On");
    tft.fillRect(145, 190, 230, CHAR_HEIGHT, RA8875_BLACK);  // Increased rectangle size to full erase value.

    tft.setCursor(145, 190);
    tft.print(prompt);
    tft.setCursor(280, 190);
    tft.print(*IQCorrectionFactor, 3);
  }
  *IQCorrectionFactor = correctionFactor + adjMinIndex * increment;

  tft.fillRect(145, 150, 230, 80, RA8875_BLACK);
}

/*****
  Purpose: Auto Tune calibrate the receive IQ
 *****/
FLASHMEM void autotuneRec(float *amp, float *phase,
                float gain_coarse_max, float gain_coarse_min,
                float phase_coarse_max, float phase_coarse_min,
                int gain_coarse_step2_N, int phase_coarse_step2_N,
                int gain_fine_N, int phase_fine_N, bool phase_first) {
  *amp = 1.0;
  *phase = 0.0;

  if(phase_first) {
    // Step 2: phase changes in 0.01 steps from -0.2 to 0.2. Find the minimum.
    int phaseStepsCoarseN = (int)((phase_coarse_max - phase_coarse_min) / 0.01 / 2);
    tuneCalParameterRec(-phaseStepsCoarseN, phaseStepsCoarseN + 1, 0.01, phase, (char *)"IQ Phase");

    // Step 1: Gain in 0.01 steps from 0.5 to 1.5
    int gainStepsCoarseN = (int)((gain_coarse_max - gain_coarse_min) / 0.01 / 2);
    tuneCalParameterRec(-gainStepsCoarseN, gainStepsCoarseN + 1, 0.01, amp, (char *)"IQ Gain");
  } else {
    // Step 1: Gain in 0.01 steps from 0.5 to 1.5
    int gainStepsCoarseN = (int)((gain_coarse_max - gain_coarse_min) / 0.01 / 2);
    tuneCalParameterRec(-gainStepsCoarseN, gainStepsCoarseN + 1, 0.01, amp, (char *)"IQ Gain");

    // Step 2: phase changes in 0.01 steps from -0.2 to 0.2. Find the minimum.
    int phaseStepsCoarseN = (int)((phase_coarse_max - phase_coarse_min) / 0.01 / 2);
    tuneCalParameterRec(-phaseStepsCoarseN, phaseStepsCoarseN + 1, 0.01, phase, (char *)"IQ Phase");
  }

  // Step 3: Gain in 0.01 steps from 4 steps below previous minimum to 4 steps above
  tuneCalParameterRec(-gain_coarse_step2_N, gain_coarse_step2_N + 1, 0.01, amp, (char *)"IQ Gain");

  // Step 4: phase in 0.01 steps from 4 steps below previous minimum to 4 steps above
  tuneCalParameterRec(-phase_coarse_step2_N, phase_coarse_step2_N + 1, 0.01, phase, (char *)"IQ Phase");

  // Step 5: gain in 0.001 steps 10 steps below to 10 steps above
  tuneCalParameterRec(-gain_fine_N, gain_fine_N + 1, 0.001, amp, (char *)"IQ Gain");

  // Step 6: phase in 0.001 steps 10 steps below to 10 steps above
  tuneCalParameterRec(-phase_fine_N, phase_fine_N + 1, 0.001, phase, (char *)"IQ Phase");
}

FLASHMEM void ShowReceiveCalibrationDisplay() {
  int menuY = iqCorIncY;

  // clear screen
  ClearScreen();

  // display menu
  tft.setFontScale((enum RA8875tsize)0);
  tft.setTextColor(RA8875_YELLOW);
  tft.setCursor(menuX, menuY);
  tft.print("11: IQ inc");
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
  tft.setCursor(menuX, menuY);
  tft.print("17: Cancel");
  menuY += 15;
  tft.setCursor(menuX, menuY);
  tft.print("Select: Save/Exit");
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
  tft.setCursor(menuX, menuY);
  tft.print("Fine: Atten adj");
  menuY += 15;
  tft.setCursor(menuX, menuY);
  tft.print("Center: NF adj");
  menuY += 15;
  tft.setCursor(menuX, menuY);
  tft.print("Band+/-: Change band");

  // display IQ adjust increment
  DisplayIQAdjustIncrement(20);

  ShowOperatingStats();
  ShowTransmitReceiveStatus();
  ShowSpectrumdBScale();
  DrawSpectrumFrame();
  ShowSpectrumFreqValues();

  // display IQ factors and adjustment dB
  tft.setFontScale((enum RA8875tsize)1);
  tft.setTextColor(RA8875_WHITE);
  tft.setCursor(menuX, 320);
  tft.print("IQ Gain");
  tft.setCursor(menuX, 360);
  tft.print("IQ Phase");
  UpdateIQDisplay();
  tft.setCursor(menuX, 400);
  tft.setTextColor(RA8875_WHITE);
  tft.print("Out Atten");
  tft.setCursor(680, 400);
  tft.setTextColor(RA8875_GREEN);
  tft.print(RECIEVE_CAL_START_ATTEN / 2.0, 1);
  tft.setCursor(menuX, 440);
  tft.setTextColor(RA8875_WHITE);
  tft.print("adjdB= ");
  tft.setCursor(680, 440);
  tft.print(aveAdjdB2, 1);
}

FLASHMEM void AutoReceiveCal() {
  autotuneRec(&IQAmpCorrectionFactor[currentBand], &IQPhaseCorrectionFactor[currentBand],
              GAIN_COARSE_MAX, GAIN_COARSE_MIN,
              PHASE_COARSE_MAX, PHASE_COARSE_MIN,
              GAIN_COARSE_STEP2_N, PHASE_COARSE_STEP2_N,
              GAIN_FINE_N, PHASE_FINE_N, false);
  tft.setFontScale((enum RA8875tsize)1);
  tft.setTextColor(RA8875_WHITE);
  tft.setCursor(145, 150);
  tft.print("Auto Complete");
  UpdateIQDisplay();
}

/*****
  Purpose: Combined input/output to calibrate the receive IQ
 *****/
FLASHMEM void CalibrateReceiveIQ() {
  int recCalFlag = 1; // 1 = do calibration, 0 = done
  int val, bandCalBand;

  // Save the current operating state to restore later
  // and configure radio for receive calibration
  CalibratePreamble(1, CW_TRANSMIT_STRAIGHT_STATE, CALIBRATE_RECEIVE_STATE);

  ShowReceiveCalibrationDisplay();

  adjdB = 0;

  // Receive calibration loop
  while(true) {
    if(recCalFlag == 0) {
      // receive calibration has finished
      // clean up and exit
      CalibratePost(1);
      break;
    }

    // process spectrum for calibration
    adjdB = ShowSpectrum2();

    // check for and process button input
    val = ReadSelectedPushButton();
    if(val != BOGUS_PIN_READ) {
      val = ProcessButtonPress(val);
    }

    switch(val) {
      case MENU_OPTION_SELECT: // 0
        // save and exit
        EEPROMWrite();
        recCalFlag = 0;
        break;

      case BAND_UP: // 2
        ChangeBand(1);
        t41.CenterFreq = bands[currentBand].calFreq;
        SetFreq(t41.CenterFreq);
        si5351.set_freq((t41.CenterFreq + calFreqOffset) * SI5351_FREQ_MULT, SI5351_CLK2);
        UpdateIQDisplay();
        break;

      case BAND_DN: // 3
        ChangeBand(-1);
        t41.CenterFreq = bands[currentBand].calFreq;
        SetFreq(t41.CenterFreq);
        si5351.set_freq((t41.CenterFreq + calFreqOffset) * SI5351_FREQ_MULT, SI5351_CLK2);
        UpdateIQDisplay();
        break;

      case NOISE_FLOOR: // 11
        // toggle IQ calibration increment
        iqIncrementIndex++;
        if(iqIncrementIndex >= 3) iqIncrementIndex = 0;
        iqCorInc = iqIncrementValues[iqIncrementIndex];

        DisplayIQAdjustIncrement(20);
        break;

      case DECODER_TOGGLE: // 13
        // auto calibrate all bands, starting with the first band
        bandCalBand = currentBand; // save current band
        ChangeBand(BAND_80M - currentBand);
        for(int i = BAND_80M; i < NUMBER_OF_BANDS; i++) {
          if(bands[currentBand].calFreq > 0) {
            t41.CenterFreq = bands[currentBand].calFreq;
            SetFreq(t41.CenterFreq);
            si5351.set_freq((t41.CenterFreq + calFreqOffset) * SI5351_FREQ_MULT, SI5351_CLK2);
            UpdateIQDisplay();
            delay(1000); // allow frequency to stabilize
            AutoReceiveCal();
          }
          ChangeBand(1);
        }
        // return to original band
        ChangeBand(bandCalBand - currentBand);
        t41.CenterFreq = bands[currentBand].calFreq;
        SetFreq(t41.CenterFreq);
        si5351.set_freq((t41.CenterFreq + calFreqOffset) * SI5351_FREQ_MULT, SI5351_CLK2);
        UpdateIQDisplay();
        break;

      case MAIN_TUNE_INCREMENT: // 14
        // start auto calibration
        AutoReceiveCal();
        break;

      case RESET_TUNING: // 15
        // toggle attenuator flag
        outputAttenAdjustActiveFlag = !outputAttenAdjustActiveFlag;
        PrintAtten();
        break;

      case BEARING: // 17
        // cancel calibration
        IQAmpCorrectionFactor[currentBand] = userIQAmpFactor;
        IQPhaseCorrectionFactor[currentBand] = userIQPhaseFactor;
        recCalFlag = 0;
        break;

      default:
        break;
    }
  }
}

//-------------------------------------------------------------------------------------------------------------
// Transmit IQ Calibration
//-------------------------------------------------------------------------------------------------------------

FLASHMEM void AdjustTransmitCalFactors() {
  AdjustIQFactors(false);
  AdjustRxTxAtten();

  // adjust image value
  ProcessCenterTuneEncoder(READ_CENTERTUNE_ENCODER);
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

FLASHMEM void ShowTransmitCalibrationMenu() {
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

FLASHMEM void DrawIQGainPlot() {
  // erase only plot
  tft.fillRect(0, 185, 480, 294, RA8875_BLACK);

  // draw gain correction plot
  tft.drawRect(0, 185, 480, 294, RA8875_GREEN);
  tft.drawFastHLine(40, 430, 400, RA8875_GREEN);
  tft.drawFastVLine(40, 200, 230, RA8875_GREEN);

  tft.setFontScale((enum RA8875tsize)0);
  tft.setTextColor(RA8875_WHITE);
  tft.setCursor(13, 275);
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
    tft.setCursor(10, 190 + k * 57.5);
    tft.print(SIG_STRENGTH_MAX * (1 - (float)k / 4.0), 0);
    tft.drawFastHLine(33, 200 + k * 57.5, 7, RA8875_GREEN);
  }

  tft.setCursor(0, 135);
  tft.print("Min Points:");
}

// plotType: type of adjustment: false = phase; true = amplitude
FLASHMEM void PlotIQGainValue(float sigStr, bool plotType = true, bool dbm = false) {
  char msg[60], f1[10], f2[10];
  int color;
  float plotX, plotY, plotValue;
  static float minGainX = 0.0, minGainY = 80.0;
  static float minPhaseX = 0.0, minPhaseY = 80.0;

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
}

FLASHMEM void ShowTransmitCalibrationDisplay() {
  ClearScreen();

  // display instructions
  tft.setFontScale((enum RA8875tsize)1);
  tft.setTextColor(RA8875_WHITE);
  tft.setCursor(100, 10);
  tft.print("Calibrate TX IQ");

  ShowTransmitCalibrationMenu();

  // display IQ factors and image value
  tft.setFontScale((enum RA8875tsize)1);
  tft.setTextColor(RA8875_WHITE);
  tft.setCursor(menuX, 320);
  tft.print("IQ Gain");
  tft.setCursor(menuX, 360);
  tft.print("IQ Phase");

  tft.setCursor(menuX, 400);
  tft.print("Sig Str");
  tft.setCursor(menuX, 440);
  tft.print("Plot Val");
  tft.fillRect(680, 440, 150, tft.getFontHeight(), RA8875_BLACK);
  tft.setTextColor(RA8875_GREEN);
  tft.setCursor(680, 440);
  tft.print(plotValue, 1);

  UpdateIQDisplay(false);

  //DrawIQGainPlot();

  PrintAtten();
}

FLASHMEM void ProcessTransmitCalIQData() {
  //float rfGainValue;
  static float theta = -2 * PI;
  float tmp;
  const float thetaInc = 2.0 * PI * intermediateFreq / sampleRate;

  if((uint32_t)Q_in_L.available() > 16 && (uint32_t)Q_in_R.available() > 16) {
    for(unsigned i = 0; i < 16; i++) {
      /**********************************************************************************
          Using arm_Math library, convert to float one buffer_size.
          Float_buffer samples are now standardized from > -1.0 to < 1.0
      **********************************************************************************/
      arm_q15_to_float (Q_in_R.readBuffer(), &audioBufferL[128 * i], 128); // convert int_buffer to float 32bit
      arm_q15_to_float (Q_in_L.readBuffer(), &audioBufferR[128 * i], 128); // convert int_buffer to float 32bit
      //arm_q15_to_float (Q_in_L.readBuffer(), &audioBufferL[128 * i], 128); // convert int_buffer to float 32bit
      //arm_q15_to_float (Q_in_R.readBuffer(), &audioBufferR[128 * i], 128); // convert int_buffer to float 32bit
      Q_in_L.freeBuffer();
      Q_in_R.freeBuffer();
    }

    //rfGainValue = pow(10, (float)rfGainAllBands / 20);
    //arm_scale_f32(audioBufferL, rfGainValue, audioBufferL, 2048);
    //arm_scale_f32(audioBufferR, rfGainValue, audioBufferR, 2048);

    //arm_scale_f32(audioBufferL, bands[currentBand].rfGain, audioBufferL, 2048);
    //arm_scale_f32(audioBufferR, bands[currentBand].rfGain, audioBufferR, 2048);

    // Manual IQ amplitude correction
    if(currentDemodMode == DEMOD_LSB || currentDemodMode == DEMOD_AM || currentDemodMode == DEMOD_SAM) {
      arm_scale_f32(audioBufferL, -IQAmpCorrectionFactor[currentBand], audioBufferL, 2048);
      IQPhaseCorrection(audioBufferL, audioBufferR, IQPhaseCorrectionFactor[currentBand], 2048);
    } else {
      if(currentDemodMode == DEMOD_USB || currentDemodMode == DEMOD_AM || currentDemodMode == DEMOD_SAM) {
      //if(currentDemodMode == DEMOD_USB || currentDemodMode == DEMOD_FT8 || currentDemodMode == DEMOD_AM || currentDemodMode == DEMOD_SAM) {
        arm_scale_f32(audioBufferL, -IQAmpCorrectionFactor[currentBand], audioBufferL, 2048);
        IQPhaseCorrection(audioBufferL, audioBufferR, IQPhaseCorrectionFactor[currentBand], 2048);
      }
    }


    if(currentDemodMode == DEMOD_LSB) {
      arm_scale_f32(audioBufferL, IQXAmpCorrectionFactor[currentBand], audioBufferL, 2048);
    }
    else if(currentDemodMode == DEMOD_USB) {
      arm_scale_f32(audioBufferL, -IQXAmpCorrectionFactor[currentBand], audioBufferL, 2048);
    }
    IQPhaseCorrection(audioBufferL, audioBufferR, IQXPhaseCorrectionFactor[currentBand], 2048);


    for(int i = 0; i< 2048; i++) {
      theta += thetaInc;
      if(theta > 2 * PI) theta = -2 * PI;
      tmp = arm_sin_f32(theta);
      //audioBufferL[i] += tmp;
      //audioBufferR[i] += tmp;
      audioBufferL[i] = (tmp - audioBufferL[i]) * 0.5;
      audioBufferR[i] = (tmp - audioBufferR[i]) * 0.5;
    }


    //CalcZoomFreqSpec(2048);
    //Calc1xFreqSpec();
  }
}

FLASHMEM void PlotSpectrum() {
  int y_new_plot, y1_new_plot, y_old_plot, y1_old_plot;
  static int oldNF = currentNF;
  int hLo = 0, hHi = 0;
  //static bool showData = true;

  // set current noise flow level for this loop
  // noise floor is constant for each spectrum update
  // this allows live noise floor updates
  if(liveNoiseFloorFlag != 1) {
    currentNF = currentNoiseFloor[currentBand];
  }

  // initialize old noise floor if this is a new spectrum
  if(newSpectrumFlag == 0) {
    oldNF = currentNF;
    newSpectrumFlag = 1;
  }

  // Draw the main Spectrum, Waterfall and Audio displays
  for(int x1 = 0; x1 < SPECTRUM_RES - 1; x1++) {
  //for(int x1 = 200; x1 < 300; x1++) {
  //for(int x1 = 230; x1 < 280; x1++) {
    // Update the frequency here only.  This is the beginning of the 512 wide spectrum display

    PrepareMicExciterData();
    //if(x1 == 0) {
    //if(x1 == 200) {
    if(x1 == 230) {
      ProcessTransmitCalIQData();
    }

    // pixelold spectrum is saved by the FFT function prior to a new FFT which generates the pixelnew spectrum
    y_new_plot = spectrumNoiseFloor - pixelnew[x1] - currentNF;
    y1_new_plot = spectrumNoiseFloor - pixelnew[x1 + 1] - currentNF;
    y_old_plot = spectrumNoiseFloor - pixelold[x1] - oldNF;
    y1_old_plot = spectrumNoiseFloor - pixelold[x1 + 1] - oldNF;

    // create rough spectrum histogram if auto noise floor is active
    // the frequency spectrum is 150 pixels high, let's create
    // rough histogram 30 bins wide (or 5 pixels each, ie, divide by 5)
    // you might think divide by 4 would be more efficient as 2 right shifts
    // but right shift of a negative number is implimentation specific
    // and I want to keep the negative numbers here
    if(liveNoiseFloorFlag == 1) {
      int specPlotY = spectrumNoiseFloor - y_new_plot; // actual spectrum value at current noise floor
      int bin = specPlotY / 5;                         // divide by 5 to get histogram bin

      // hLo and hHi capture spectrum at or outside the spectrum display extremes
      // this is all we need to automatically set the noise floor
      // *** TODO: consider using other histogram bins to more rapidly set noise flow ***
      if(bin < 1) {
        hLo += 1;
      } else if(bin >= 29) {
        hHi += 1;
      }
    }

    // Prevent spectrum from going below the bottom of the spectrum area
    //if(y_new_plot > SPECTRUM_BOTTOM) y_new_plot = SPECTRUM_BOTTOM;
    //if(y1_new_plot > SPECTRUM_BOTTOM) y1_new_plot = SPECTRUM_BOTTOM;
    //if(y_old_plot > SPECTRUM_BOTTOM) y_old_plot = SPECTRUM_BOTTOM;
    //if(y1_old_plot > SPECTRUM_BOTTOM) y1_old_plot = SPECTRUM_BOTTOM;

    // Prevent spectrum from going above the top of the spectrum area
    //if(y_new_plot < SPECTRUM_TOP_Y) y_new_plot = SPECTRUM_TOP_Y;
    //if(y1_new_plot < SPECTRUM_TOP_Y) y1_new_plot = SPECTRUM_TOP_Y;
    //if(y_old_plot < SPECTRUM_TOP_Y) y_old_plot = SPECTRUM_TOP_Y;
    //if(y1_old_plot < SPECTRUM_TOP_Y) y1_old_plot = SPECTRUM_TOP_Y;

    if(y_new_plot > 480) y_new_plot = 480;
    if(y1_new_plot > 480) y1_new_plot = 480;
    if(y_old_plot > 480) y_old_plot = 480;
    if(y1_old_plot > 480) y1_old_plot = 480;
    if(y_new_plot < 0) y_new_plot = 0;
    if(y1_new_plot < 0) y1_new_plot = 0;
    if(y_old_plot < 0) y_old_plot = 0;
    if(y1_old_plot < 0) y1_old_plot = 0;

    // Erase the old spectrum, and draw the new spectrum.
    tft.drawLine(SPECTRUM_LEFT_X + x1, y1_old_plot, SPECTRUM_LEFT_X + x1, y_old_plot, RA8875_BLACK);
    tft.drawLine(SPECTRUM_LEFT_X + x1, y1_new_plot, SPECTRUM_LEFT_X + x1, y_new_plot, RA8875_YELLOW);

    //if(showData) {
    //  static int count = 0;
    //  if(x1 == 279 && count++ > 500) {
    //    showData = false;
    //  }
    //  Serial.print(x1); Serial.print(", "); Serial.println(SPECTRUM_BOTTOM - y_new_plot);
    //}

    // What is the actual spectrum at this time?  It's a combination of the old and new spectrums
    // In the case of a CW interrupt, the array pixelnew should be saved as the actual spectrum
    // This is the actual "old" spectrum!  This is required due to CW interrupts
    // pixelCurrent gets copied to pixelold by the FFT function
    pixelCurrent[x1] = pixelnew[x1];
  }

  // update S-meter once per loop
  //DrawSmeterBar();

  pixelCurrent[279] = pixelnew[279];

  oldNF = currentNF; // save the noise floor we used for this spectrum

  //if(connected) {
  //  freqData[511] = pixelnew[SPECTRUM_RES - 1];
  //}

  // adjust noise floor if auto noise floor is active
  if(liveNoiseFloorFlag == 1) {
    // auto noise floor give priority to ensuring the noise floor is visible in the lower portion of the spectrum display
    // the spectrum is 512 pixels wide, the noise floor is adjusted as follows (in order of priority):
    //    1) increased if more than a 20% of the spectrum is the bottom bin
    //    2) decreased if more than 5% is in the top bin
    //    3) decrease if less than 10% is in bottom bin
    // *** TODO: consider using other histogram bins to more rapidly set noise flow ***
    if(hLo > 102) {
      currentNF += 1;
    } else if((hHi > 25) || (hLo < 51)) {
      currentNF -= 1;
    }
  }
}

float minSignalStrength;

FLASHMEM void UpdateAutoCalDisplay() {
  tft.setFontScale((enum RA8875tsize)1);
  tft.fillRect(680, 400, 150, tft.getFontHeight(), RA8875_BLACK);
  tft.fillRect(680, 440, 150, tft.getFontHeight(), RA8875_BLACK);
  tft.setTextColor(RA8875_YELLOW);
  tft.setCursor(680, 400);
  tft.print(signalStrength, 1);
  tft.setCursor(680, 440);
  tft.print(minSignalStrength, 1);

  UpdateIQDisplay(false, true);
}

FLASHMEM bool GetSignalStrength(float *pSS) {
  bool result = false;
  unsigned long prevMillis = millis();
  unsigned long lastSampleMillis = prevMillis;
  int ssIndex = 0;
  float samples[3], stdev;
  bool getNextSample = true;
  //static int count = 0;

  // reset globals
  signalStrengthReceivedIndex = -1;
  signalStrengthReceived = false;
  while(!result) {
    if(digitalRead(PTT) == HIGH) break;

    PrepareMicExciterData();
    T41ControlLoop();

    // process sample if it's index matches the current index
    if(signalStrengthReceived && signalStrengthReceivedIndex ==  ssIndex) {
      // collect 3 samples
      samples[ssIndex++] = signalStrength;

      if(ssIndex == 3) {
        // we've got all 3 samples, process
        arm_std_f32(samples, 3, &stdev);

        if(stdev > 1.0) {
          // standard deviation is too high, start again
          ssIndex = 0;
          getNextSample = true;
        } else {
          arm_mean_f32(samples, 3, pSS);
          result = true;
        }
      } else {
        getNextSample = true;
      }

      signalStrengthReceived = false;
      signalStrengthReceivedIndex = -1;
      UpdateAutoCalDisplay();
    }

    // request signal strength when ready or every 5 seconds
    if(!result) {
      if((getNextSample && millis() - lastSampleMillis > 100) || (millis() - prevMillis > 5000)) {
        SendSignalStrengthRequest(ssIndex);
        prevMillis = lastSampleMillis = millis();
        getNextSample = false;

        //Serial.print(".");
        //if(count++ > 100) {
        //  Serial.println();
        //  count = 0;
        //}
      }
    }

    if(digitalRead(PTT) == HIGH) break;
  }

  return result;
}

FLASHMEM void tuneCalParameterTran(int indexStart, int indexEnd, float increment, float *IQCorrectionFactor) {
  int minIndex = 0;
  int index = indexStart;
  float correctionFactor = *IQCorrectionFactor;
  unsigned long prevMillis = millis();
  float meanSignalStrength;

  //GetSignalStrength(&meanSignalStrength);

  // reset globals
  *IQCorrectionFactor = correctionFactor + index * increment;
  PrepareMicExciterData();
  signalStrengthReceivedIndex = -1;
  signalStrengthReceived = false;
  while(index < indexEnd) {
    if(digitalRead(PTT) == HIGH) return;

    if(GetSignalStrength(&meanSignalStrength)) {
      if(meanSignalStrength < minSignalStrength) {
        minSignalStrength = meanSignalStrength;
        minIndex = index;
        //Serial.println();
        //Serial.print("min factor "); Serial.print(correctionFactor + index * increment); Serial.print(" found @: "); Serial.println(minSignalStrength);
      }

      PlotIQGainValue(meanSignalStrength, IQCorrectionFactor == &IQXAmpCorrectionFactor[currentBand], true);

      // update IQ correction factor for next increment
      index++;
      *IQCorrectionFactor = correctionFactor + index * increment;
      PrepareMicExciterData();
      UpdateAutoCalDisplay();

      // allow radio to stabilize
      while(millis() - prevMillis < 100) {
        PrepareMicExciterData();
        T41ControlLoop(); // clean up any outstanding replies
      }
    } else {
      //Serial.println(); Serial.println("Autotune aborted"); Serial.println();
      return;
    }
  }

  *IQCorrectionFactor = correctionFactor + minIndex * increment;
  UpdateAutoCalDisplay();
}

FLASHMEM void autotuneTran(float *amp, float *phase, float gain_coarse_max, float gain_coarse_min,
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
  tft.print("Sig Str");
  tft.setCursor(menuX, 440);
  tft.print("Min Sig");
  tft.setFontScale((enum RA8875tsize)0);
  tft.setCursor(menuX, 280);
  tft.print("  starting auto cal...");
  tft.setTextColor(RA8875_YELLOW);
  tft.setCursor(menuX, 295);
  tft.print("  (release PTT to cancel)");

  if(!GetSignalStrength(&minSignalStrength)) return;
  //Serial.println();
  //Serial.print("Starting auto cal - minimum signal strength: "); Serial.println(minSignalStrength);
  UpdateAutoCalDisplay();

  if(phase_first) {
    // Step 2: phase changes in 0.01 steps from -0.2 to 0.2. Find the minimum.
    int phaseStepsCoarseN = (int)((phase_coarse_max - phase_coarse_min) / 0.01 / 2);
    //Serial.print("Step 2: ");
    tuneCalParameterTran(-phaseStepsCoarseN, phaseStepsCoarseN + 1, 0.01, phase);
    // Step 1: Gain in 0.01 steps from 0.5 to 1.5
    int gainStepsCoarseN = (int)((gain_coarse_max - gain_coarse_min) / 0.01 / 2);
    //Serial.print("Step 1: ");
    tuneCalParameterTran(-gainStepsCoarseN, gainStepsCoarseN + 1, 0.01, amp);
  } else {
    // Step 1: Gain in 0.01 steps from 0.5 to 1.5
    int gainStepsCoarseN = (int)((gain_coarse_max - gain_coarse_min) / 0.01 / 2);

    tft.setFontScale((enum RA8875tsize)0);
    tft.fillRect(menuX, 280, 250, tft.getFontHeight(), RA8875_BLACK);
    tft.setTextColor(RA8875_YELLOW);
    tft.setCursor(menuX, 280);
    tft.print("  1. Adjusting course gain...");

    //Serial.println(); Serial.println("Step 1: ");
    tuneCalParameterTran(-gainStepsCoarseN, gainStepsCoarseN + 1, 0.01, amp);

    // Step 2: phase changes in 0.01 steps from -0.2 to 0.2. Find the minimum.
    int phaseStepsCoarseN = (int)((phase_coarse_max - phase_coarse_min) / 0.01 / 2);

    tft.setFontScale((enum RA8875tsize)0);
    tft.fillRect(menuX, 280, 250, tft.getFontHeight(), RA8875_BLACK);
    tft.setTextColor(RA8875_YELLOW);
    tft.setCursor(menuX, 280);
    tft.print("  2. Adjusting course phase...");

    //Serial.println(); Serial.println("Step 2: ");
    tuneCalParameterTran(-phaseStepsCoarseN, phaseStepsCoarseN + 1, 0.01, phase);
  }

  // Step 3: Gain in 0.01 steps from 4 steps below previous minimum to 4 steps above
  tft.setFontScale((enum RA8875tsize)0);
  tft.fillRect(menuX, 280, 250, tft.getFontHeight(), RA8875_BLACK);
  tft.setTextColor(RA8875_YELLOW);
  tft.setCursor(menuX, 280);
  tft.print("  3. Adjusting gain...");

  //Serial.println(); Serial.println("Step 3: ");
  tuneCalParameterTran(-gain_coarse_step2_N, gain_coarse_step2_N + 1, 0.01, amp);

  // Step 4: phase in 0.01 steps from 4 steps below previous minimum to 4 steps above
  tft.setFontScale((enum RA8875tsize)0);
  tft.fillRect(menuX, 280, 250, tft.getFontHeight(), RA8875_BLACK);
  tft.setTextColor(RA8875_YELLOW);
  tft.setCursor(menuX, 280);
  tft.print("  4. Adjusting phase...");

  //Serial.println(); Serial.print("Step 4: ");
  tuneCalParameterTran(-phase_coarse_step2_N, phase_coarse_step2_N + 1, 0.01, phase);

  // Step 5: gain in 0.001 steps 10 steps below to 10 steps above
  tft.setFontScale((enum RA8875tsize)0);
  tft.fillRect(menuX, 280, 250, tft.getFontHeight(), RA8875_BLACK);
  tft.setTextColor(RA8875_YELLOW);
  tft.setCursor(menuX, 280);
  tft.print("  5. Adjusting fine gain...");

  //Serial.println(); Serial.println("Step 5: ");
  tuneCalParameterTran(-gain_fine_N, gain_fine_N + 1, 0.001, amp);

  // Step 6: phase in 0.001 steps 10 steps below to 10 steps above
  tft.setFontScale((enum RA8875tsize)0);
  tft.fillRect(menuX, 280, 250, tft.getFontHeight(), RA8875_BLACK);
  tft.setTextColor(RA8875_YELLOW);
  tft.setCursor(menuX, 280);
  tft.print("  6. Aadjusting fine phase...");

  //Serial.println(); Serial.println("Step 6: ");
  tuneCalParameterTran(-phase_fine_N, phase_fine_N + 1, 0.001, phase);

  tft.setFontScale((enum RA8875tsize)0);
  tft.fillRect(menuX, 280, 250, 2 * tft.getFontHeight(), RA8875_BLACK);
  tft.setTextColor(RA8875_WHITE);
  tft.setCursor(menuX, 280);
  tft.print("  Auto Cal Done");

  //Serial.println();
  //Serial.println("Auto cal complete");
}

FLASHMEM void AutoTransmitCal() {
  autotuneTran(&IQXAmpCorrectionFactor[currentBand], &IQXPhaseCorrectionFactor[currentBand],
              GAIN_COARSE_MAX, GAIN_COARSE_MIN,
              PHASE_COARSE_MAX, PHASE_COARSE_MIN,
              GAIN_COARSE_STEP2_N, PHASE_COARSE_STEP2_N,
              GAIN_FINE_N, PHASE_FINE_N, false);
  UpdateIQDisplay(false);
}

void SetupSignalStrengthSource(int source) {
  unsigned long prevMillis;

  // set up signal strength source
  switch(source) {
    case 0: // manual
      tft.setTextColor(RA8875_WHITE);
      tft.setCursor(menuX, 400);
      tft.print("Sig Str");
      tft.setCursor(menuX, 440);
      tft.print("Plot Val");
      //tft.fillRect(680, 440, 150, tft.getFontHeight(), RA8875_BLACK);
      PrintAtten(); // v12
      break;

    case 1: // external
      // set up this and external unit for calibration
      minSignalStrength = 0;
      signalStrengthSource = 1;
      SendCenterFreq(t41.CenterFreq + intermediateFreq);
      if(currentDemodMode == DEMOD_LSB) {
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
      tft.print("Sig Str");
      tft.setCursor(menuX, 440);
      tft.print("Min Sig");

      UpdateAutoCalDisplay();
      break;

    case 2: // loopback
    default:
      break;
  }
}

/*****
  Purpose: Combined input/output to calibrate the transmit IQ
 *****/
FLASHMEM void CalibrateTransmitIQ() {
  int tranCalFlag = 1; // 1 = do calibration, 0 = done
  int val, bandCalBand;
  unsigned long prevMillis;
  //int priorInAtten = 0, priorOutAtten = 0;
  //bool calRelayOn = false;

  // set up for tranmit calibration
  CalibratePreamble(2, CALIBRATE_TRANSMIT_STATE, CALIBRATE_TRANSMIT_STATE);

  ShowTransmitCalibrationDisplay();

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

  // transmit calibration loop
  while(1) {
    if(tranCalFlag == 0) {
      // transmit calibration has finished
      // clean up and exit
      CalibratePost(2);
      break;
    }

    ShowTransmitReceiveStatus();
    AdjustTransmitCalFactors();

    if(digitalRead(PTT) == LOW) {
      digitalWrite(RXTX, HIGH);

      PrepareMicExciterData();
      T41ControlLoop();

      if(signalStrengthSource == 1) {
        // request signal strength every 5 seconds
        if(millis() - prevMillis > 5000) {
          signalStrengthReceived = false;
          SendSignalStrengthRequest();
          prevMillis = millis();
        }
        if(signalStrengthReceived) {
          //Serial.print(i); Serial.print(": ");
          //Serial.println(signalStrength);
          if(signalStrength < minSignalStrength) {
            minSignalStrength = plotValue = signalStrength;
          }
          UpdateAutoCalDisplay();
          signalStrengthReceived = false;
        }
      }
    } else {
      digitalWrite(RXTX, LOW);

      Q_in_L.clear();
      Q_in_R.clear();
    }

    // check for and process button input
    val = ReadSelectedPushButton();
    if(val != BOGUS_PIN_READ) {
      val = ProcessButtonPress(val);
    }

    switch(val) {
      case MENU_OPTION_SELECT: // 0
        // save and exit
        EEPROMWrite();
        tranCalFlag = 0;
        break;

      case BAND_UP: // 2
        ChangeBand(1);
        t41.CenterFreq = bands[currentBand].calFreq;
        SetFreq(t41.CenterFreq);
        si5351.set_freq((t41.CenterFreq + calFreqOffset) * SI5351_FREQ_MULT, SI5351_CLK2);
        UpdateIQDisplay();
        break;

      case BAND_DN: // 3
        ChangeBand(-1);
        t41.CenterFreq = bands[currentBand].calFreq;
        SetFreq(t41.CenterFreq);
        si5351.set_freq((t41.CenterFreq + calFreqOffset) * SI5351_FREQ_MULT, SI5351_CLK2);
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

        // set up signal strength source
        SetupSignalStrengthSource(signalStrengthSource);
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
            t41.CenterFreq = bands[currentBand].calFreq;
            SetFreq(t41.CenterFreq);
            si5351.set_freq((t41.CenterFreq + calFreqOffset) * SI5351_FREQ_MULT, SI5351_CLK2);
            UpdateIQDisplay();

            // set up signal strength source
            SetupSignalStrengthSource(signalStrengthSource);

            // auto calibrate this band
            AutoTransmitCal();

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
        t41.CenterFreq = bands[currentBand].calFreq;
        SetFreq(t41.CenterFreq);
        si5351.set_freq((t41.CenterFreq + calFreqOffset) * SI5351_FREQ_MULT, SI5351_CLK2);
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
        AutoTransmitCal();
        break;

      case RESET_TUNING: // 15
        // toggle attenuator flag
        outputAttenAdjustActiveFlag = !outputAttenAdjustActiveFlag;
        PrintAtten();
        break;

      case BEARING: // 17
        // cancel calibration
        IQXAmpCorrectionFactor[currentBand] = userIQAmpFactor;
        IQXPhaseCorrectionFactor[currentBand] = userIQPhaseFactor;
        tranCalFlag = 0;
        break;

      default:
        break;
    }
  }
}

//-------------------------------------------------------------------------------------------------------------
// Two Tone Test
//-------------------------------------------------------------------------------------------------------------

FLASHMEM void DisplayTones(int cycles1, int cycles2) {
  tft.setFontScale((enum RA8875tsize)1);
  tft.fillRect(680, 400, 150, tft.getFontHeight(), RA8875_BLACK);
  tft.fillRect(680, 440, 150, tft.getFontHeight(), RA8875_BLACK);
  tft.setTextColor(RA8875_GREEN);
  tft.setCursor(680, 400);
  tft.print(cycles1 * sampleRate / 8.0 / 256.0, 0);
  tft.setCursor((float)680, 440);
  tft.print((float)cycles2 * sampleRate / 8.0 / 256.0, 0);
}

FLASHMEM void IncTone(int tone, int inc = 0) {
  static int numTwoToneCycles1 = 8;
  static int numTwoToneCycles2 = 20;

  switch(tone) {
    case 1:
      numTwoToneCycles1 += inc * 4;
      if(numTwoToneCycles1 < 4) numTwoToneCycles1 = 4;
      if(numTwoToneCycles1 > 36) numTwoToneCycles1 = 36;
      GenTwoToneBuffer(numTwoToneCycles1, 1);
      break;

    case 2:
      numTwoToneCycles2 += inc * 4;
      if(numTwoToneCycles2 < 4) numTwoToneCycles2 = 4;
      if(numTwoToneCycles2 > 36) numTwoToneCycles2 = 36;
      GenTwoToneBuffer(numTwoToneCycles2, 2);
      break;

      default:
      break;
  }
  DisplayTones(numTwoToneCycles1, numTwoToneCycles2);
}

FLASHMEM void AdjustTwoToneFactors() {
  AdjustIQFactors(false);

  // tone 2 change
  if(fineTuneEncoderMove != 0) {
    IncTone(2, fineTuneEncoderMove);
    fineTuneEncoderMove = 0;
  }

  // tone 1 change
  ProcessCenterTuneEncoder(READ_CENTERTUNE_ENCODER);
  if(tuneChange != 0) {
    IncTone(1, tuneChange);
    tuneChange = 0;
  }
}

FLASHMEM void ShowTwoToneDisplay() {
  int menuY = iqCorIncY;

  ClearScreen();

  // display instructions
  tft.setFontScale((enum RA8875tsize)1);
  tft.setTextColor(RA8875_WHITE);
  tft.setCursor(100, 10);
  tft.print("Two Tone Test");
  tft.setFontScale((enum RA8875tsize)0);
  tft.setTextColor(RA8875_CYAN);
  tft.setCursor(10, 50);
  tft.print("Directions");
  tft.setCursor(10, 65);
  tft.print("* Calibrate t41 xmit IQ");
  tft.setCursor(25, 80);
  tft.print("* Attach T41 output to Dummy load/Attenuator");
  tft.setCursor(25, 95);
  tft.print("* Attach switch to PTT");
  tft.setCursor(25, 110);
  tft.print("* Set T41 to desired frequency");
  tft.setCursor(25, 125);
  tft.print("* Set T41 SSA PA Power");
  tft.setCursor(25, 140);
  tft.print("* Attach Spectrum Analyzer to T41 thru Attenuator");
  tft.setCursor(25, 155);
  tft.print("* Set SA to center freq = T41 freq");
  tft.setCursor(25, 170);
  tft.print("* Set SA span to 20KHz or 50KHz");
  tft.setCursor(25, 185);
  tft.print("* Set SA attenuation or input level");
  tft.setCursor(25, 200);
  tft.print("* Select Calibrate/Two Tone Test from T41 Menu");
  tft.setCursor(25, 215);
  tft.print("* Select Tone Freq");
  tft.setCursor(25, 230);
  tft.print("* Press PTT switch to Measure");
  tft.setCursor(25, 245);
  tft.print("* Read T41 output on Spectrum Analyzer");
  tft.setCursor(25, 260);
  tft.print("* Press Select to exit");
  tft.setCursor(10, 290);
  tft.print(" Optional - Adjust Xmit IQ Gain/Phase");
  tft.setCursor(25, 305);
  tft.print("* ");
  tft.setCursor(25, 320);
  tft.print("* Use Filter encoder to minimize IQ image");
  tft.setCursor(25, 335);
  tft.print("* Adjust Gain and Phase");
  tft.setCursor(25, 350);
  tft.print("* User 3 to Toggle Increment as needed");
  tft.setCursor(25, 365);
  tft.print("* Press Select to exit");

  // display menu
  tft.setFontScale((enum RA8875tsize)0);
  tft.setTextColor(RA8875_YELLOW);
  tft.setCursor(menuX, menuY);
  tft.print("11: IQ inc");
  menuY += 15;
  tft.setCursor(menuX, menuY);
  tft.print("13: All bands adj");
  menuY += 15;
  tft.setCursor(menuX, menuY);
  tft.print("14: Auto adj");
  menuY += 15;
  tft.setCursor(menuX, menuY);
  tft.print("15: RF CAL relay toggle");
  menuY += 15;
  tft.setCursor(menuX, menuY);
  tft.print("17: Cancel");
  menuY += 15;
  tft.setCursor(menuX, menuY);
  tft.print("Select: Save/Exit");
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
  tft.setCursor(menuX, menuY);
  tft.print("Fine: Tone 1 adj");
  menuY += 15;
  tft.setCursor(menuX, menuY);
  tft.print("Center: Tone 2 adj");
  menuY += 15;
  tft.setCursor(menuX, menuY);
  tft.print("Band+/-: Change band");

  // display IQ adjust increment
  DisplayIQAdjustIncrement(20);

  // display IQ factors and tones
  tft.setFontScale((enum RA8875tsize)1);
  tft.setTextColor(RA8875_WHITE);
  tft.setCursor(menuX, 320);
  tft.print("IQ Gain");
  tft.setCursor(menuX, 360);
  tft.print("IQ Phase");
  UpdateIQDisplay(false);

  tft.setCursor(menuX, 400);
  tft.setTextColor(RA8875_WHITE);
  tft.print("Tone 1");
  tft.setCursor(menuX, 440);
  tft.setTextColor(RA8875_WHITE);
  tft.print("Tone 2");
  IncTone(0);
}

const float thetaInc1 = 2.0 * PI * 700.0 / (sampleRate / 8.0);
const float thetaInc2 = 2.0 * PI * 1900.0 / (sampleRate / 8.0);

FLASHMEM void GetTwoToneData(float *bufI, float *bufQ, int len) {
  static float theta1, theta2;

  for(int i = 0; i < len; i++) {
    theta1 += thetaInc1;
    theta2 += thetaInc2;

    if(theta1 > 2 * PI) theta1 = -2 * PI;
    if(theta2 > 2 * PI) theta2 = -2 * PI;

    //bufI[i] = (arm_sin_f32(theta1) + arm_sin_f32(theta2)) * 0.25;
    bufQ[i] = (arm_cos_f32(theta1) + arm_cos_f32(theta2)) * 0.25;
    bufI[i] = bufQ[i];
  }
}

FLASHMEM void GetTwoToneData() {
  int16_t *sp_L, *sp_R;

  // process samples from queue buffer if there are at least 16 buffers available
  if((uint32_t)Q_in_L_Ex.available() > 16 && (uint32_t)Q_in_R_Ex.available() > 16) {

    // get audio samples from the audio  buffers and convert them to float
    for(unsigned i = 0; i < 16; i++) {
      // read in 16 blocks of 128 samples into the left channel, we'll duplicate this later
      sp_L = Q_in_L_Ex.readBuffer();
      sp_R = Q_in_R_Ex.readBuffer();

      // convert to float one buffer_size, samples are now standardized from > -1.0 to < 1.0
      arm_q15_to_float(sp_L, &audioBufferL_EX[128 * i], 128); // convert int_buffer to float 32bit
      arm_q15_to_float(sp_R, &audioBufferR_EX[128 * i], 128); // convert int_buffer to float 32bit

      Q_in_L_Ex.freeBuffer();
      Q_in_R_Ex.freeBuffer();
    }

    // reduce sample rate and size by decimation by 8
    // decimate in two stages to maintain spectrum order
    // 192kHz effective sample rate here
    // decimation-by-4 in-place
    arm_fir_decimate_f32(&FIR_dec1_EX_I, audioBufferL_EX, audioBufferL_EX, 128 * 16 );
    arm_fir_decimate_f32(&FIR_dec1_EX_Q, audioBufferR_EX, audioBufferR_EX, 128 * 16 );

    // 48KHz effective sample rate here
    // decimation-by-2 in-place
    arm_fir_decimate_f32(&FIR_dec2_EX_I, audioBufferL_EX, audioBufferL_EX, 512);
    arm_fir_decimate_f32(&FIR_dec2_EX_Q, audioBufferR_EX, audioBufferR_EX, 512);

    // leaving us at 24kHz sample rate here

    // applying a portion of the scaling here and after the interpolation seems to give a better signal
    //arm_scale_f32(audioBufferL_EX, 8, audioBufferL_EX, 256);
    //arm_scale_f32(audioBufferR_EX, 8, audioBufferR_EX, 256);

    //arm_scale_f32(audioBufferL_EX, -1.0, audioBufferL_EX, 256);

    arm_add_f32(audioBufferL_EX, audioBufferR_EX, audioBufferL_EX, 256);

    arm_scale_f32(audioBufferL_EX, 2, audioBufferL_EX, 256);

    PrepareExciterIQData();
  }
}

FLASHMEM void PrepareExciterIQData(int mode) {
  // apply any mode specific processing
  switch(mode) {
    case 0:
      // Two-tone signal generation - uses Hilbert transfor to generate IQ signals
      //GetTwoToneData();


      //GetTwoToneData(audioBufferL_EX, audioBufferR_EX, 256);
      //arm_scale_f32(audioBufferL_EX, 0.25, audioBufferL_EX, 256);
      //arm_scale_f32(audioBufferR_EX, 0.25, audioBufferR_EX, 256);

      // scale data to proper level
      arm_add_f32(sinBuffer4, sinBuffer5, audioBufferL_EX, 256);
      //Serial.print(XAttenSSB[currentBand]); Serial.print(", "); Serial.println(sinBuffer4[100]);
      //arm_scale_f32(audioBufferL_EX, .05, audioBufferL_EX, 256);
      arm_scale_f32(audioBufferL_EX, 10.0, audioBufferL_EX, 256);
      //arm_scale_f32(audioBufferL_EX, 100.0, audioBufferL_EX, 256);
      //arm_scale_f32(audioBufferL_EX, 1000.0, audioBufferL_EX, 256);
      //arm_scale_f32(audioBufferL_EX, 0.1, audioBufferL_EX, 256);

      PrepareExciterIQData();
      break;

    case 1:
      // //Sine wave generator for transmit IQ Calibrate and Transmit SSB power calibrate
      //arm_scale_f32(sinBufferxx, .03, audioBufferL_EX, 256);
      break;

    // Passthrough
    case 2:
    default:
      break;
  }

}

/*****
  Purpose: Two Tone test from digital signals 750Hz 1875 Hz

   Parameter List:
      void

   Return value:
      void

 *****/
FLASHMEM void TwoToneTest() {
  int testFlag = 1; // 1 = do test, 0 = done
  int val;
  float ampCorFactor, phaseCorFactor;
  //float iqCorInc;
  //static int corrChange;

  CalibratePreamble(3, CALIBRATE_TWOTONE_STATE, CALIBRATE_TWOTONE_STATE);
  ampCorFactor = IQXAmpCorrectionFactor[currentBand];
  phaseCorFactor = IQXPhaseCorrectionFactor[currentBand];

  // generate tone buffers and show test display
  GenTwoToneBuffer(8, 1); // 750 Hz
  GenTwoToneBuffer(20, 2); // 1875 Hz
  ShowTwoToneDisplay();

  // *** TODO: move to AudioConfig
  //sgtl5000_1.inputSelect(AUDIO_INPUT_LINEIN);

  while(1) {
    if(testFlag == 0) {
      // test finished, clean up and exit
      CalibratePost(3);
      break;
    }

    ShowTransmitReceiveStatus();
    AdjustTwoToneFactors();

    if(digitalRead(PTT) == LOW) {
      digitalWrite(RXTX, HIGH);
      PrepareExciterIQData(0);
    } else {
      digitalWrite(RXTX, LOW);
    }

    val = ReadSelectedPushButton();
    if(val != BOGUS_PIN_READ) {
      val = ProcessButtonPress(val);

      switch(val) {
        case MENU_OPTION_SELECT: // 0
          testFlag = 0;
          break;

        case NOISE_FLOOR: // 11
          // toggle IQ calibration increment
          iqIncrementIndex++;
          if(iqIncrementIndex >= 3) iqIncrementIndex = 0;
          iqCorInc = iqIncrementValues[iqIncrementIndex];

          DisplayIQAdjustIncrement(20);
          break;

        case DECODER_TOGGLE: // 13
          // increment tone 1
          //IncTone(1);
          break;

        case MAIN_TUNE_INCREMENT: // 14
          // increment tone 2
          //IncTone(2);
          break;

        case RESET_TUNING: // 15
          // toggle calibration relay
          digitalWrite(RF_CAL_RELAY, ON);
          break;

        // Toggle gain and phase
        //case (CAL_CHANGE_TYPE):  //CAL_CHANGE_TYPE=16 User2
        //  break;

        // Toggle increment value
        //case (CAL_CHANGE_INC):  //CAL_CHANGE_INC=17 User3
        //  corrChange = !corrChange;
        //  if(corrChange == 1) {          // Toggle increment value
        //    iqCorInc = 0.001;
        //  } else {
        //    iqCorInc = 0.01;
        //  }
        //  tft.setFontScale((enum RA8875tsize)0);
        //  tft.fillRect(650, 90, 50, tft.getFontHeight(), RA8875_BLACK);
        //  tft.setTextColor(RA8875_YELLOW);
        //  tft.setCursor(650, 90);
        //  tft.print(iqCorInc, 3);
        //  break;

        case BEARING: // 17
          // cancel test
          IQXAmpCorrectionFactor[currentBand] = ampCorFactor;
          IQXPhaseCorrectionFactor[currentBand] = phaseCorFactor;
          testFlag = 0;
          break;

        default:
          break;
      }
    }
  }
}
