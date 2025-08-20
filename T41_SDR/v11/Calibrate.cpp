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

#define GAIN_STEPS 15
#define PHASE_STEPS 15
#define GAIN_FINE_STEPS 10
#define PHASE_FINE_STEPS 10

#define SIG_STRENGTH_MAX 8

// preserve/restore radio state
int userFilterLowCut, userFilterHiCut, userMode, userDemodMode, userRadioState;
int userScale, userZoomIndex, userXmtMode, userBand, userVol;
long userCenterFreq, userTxRxFreq, userNCOFreq;
float userIQAmpFactor, userIQPhaseFactor;

// common to several routines
float iqIncrementValues[] = { 0.001, 0.01, 0.1 };
int iqIncrementIndex = 1;
float iqCorInc = iqIncrementValues[iqIncrementIndex];
int iqCorIncY = 115;
int menuX = 530;
int minPointsY = 165;
int calNFAdjust = 25;
int fftBins = 5;  // the number of FFT bins to examine on either side of binCenter for the signal peak

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
  userNCOFreq = NCOFreq;
  userCenterFreq = centerFreq;
  userRadioState = radioState;
  userMode = radioMode;
  userDemodMode = bands[currentBand].demod;
  userZoomIndex = spectrumZoom;
  userBand = currentBand;
  userScale = currentScale;
  userVol = audioVolume;
  displayState = DISPLAY_CALIBRATION;

  TxRxFreq = centerFreq = centerFreq + NCOFreq;
  NCOFreq = 0;
  audioVolume = 2;

  // calibration specific configuration
  switch(calType) {
    case 0: // frequency cal
      break;

    case 1: // receive IQ cal
      //SetFreqCal(24000);
      //SetFreqCal(22000);
      SetFreqCal(0);
      SetZoom(0); // 1x
      //SetZoom(1); // 2x
      //SetZoom(2); // 4x
      userIQAmpFactor = IQAmpCorrectionFactor[currentBand];
      userIQPhaseFactor = IQPhaseCorrectionFactor[currentBand];
      break;

    case 2: // transmit IQ cal
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
  Purpose: restore radio after IQ calibrations
 *****/
FLASHMEM void CalibratePost(int calType) {
  // restore radio operating state
  TxRxFreq = userTxRxFreq;
  NCOFreq = userNCOFreq;
  centerFreq = userCenterFreq;
  radioState = userRadioState;
  radioMode = userMode;
  bands[currentBand].demod = userDemodMode;
  spectrumZoom = userZoomIndex;
  currentScale = userScale;
  volSetting = userVol;

  displayState = DISPLAY_T41;

  if(currentBand != userBand) {
    ChangeBand(userBand - currentBand);
    currentBand = userBand;
  }

  // Restore the user's zoom setting
  SetZoom(userZoomIndex); // ... and zoom display

  SetFreq();

  // reset frequency spectrum buffers
  SET_VAR(pixelnew, SPECTRUM_BOTTOM);
  InitFFTArrays();
  newSpectrumFlag = 0;

  // calibration specific restoration
  switch(calType) {
    case 0: // frequency cal
      bands[currentBand].demod = userDemodMode;
      //currentFilterLoCut = userFilterLowCut;
      //currentFilterHiCut = userFilterHiCut;
      //CalcFilters();
      break;

    case 1: // receive cal
      break;

    case 2: // transmit cal
      break;

    case 3: // two tone test
      break;

    default:
      break;
  }

  digitalWrite(RXTX, LOW);  // Turn off the transmitter.

  transmitPowerLevel = transmitPowerLevelTemp;  // Restore the user's transmit power level setting.  KF5N August 15, 2023


  // restore screen
  tft.writeTo(L2);
  tft.clearMemory();
  tft.writeTo(L1);
  tft.clearMemory();

  RedrawDisplayScreen();

  //lastState = -1; // force radio state reset
  lastState = CALIBRATE_TRANSMIT_STATE; // force radio state reset
}

FLASHMEM void ShowBand() {
  tft.setFontScale((enum RA8875tsize)0);
  tft.fillRect(menuX + 20 * tft.getFontWidth(), iqCorIncY-30, 25, tft.getFontHeight(), RA8875_BLACK);
  tft.setTextColor(RA8875_GREEN);
  tft.setCursor(menuX + 20 * tft.getFontWidth(), iqCorIncY-30);
  tft.print(bands[currentBand].name);

  tft.setTextColor(RA8875_WHITE);
  tft.fillRect(10 * tft.getFontWidth(), 0, 25, tft.getFontHeight(), RA8875_BLACK);
  tft.setCursor(10 * tft.getFontWidth(), 0);
  tft.print(bands[currentBand].name);
}

FLASHMEM void ShowPlotIncrement() {
  tft.setFontScale((enum RA8875tsize)0);
  tft.fillRect(menuX + 20 * tft.getFontWidth(), iqCorIncY, 50, tft.getFontHeight(), RA8875_BLACK);
  tft.setTextColor(RA8875_GREEN);
  tft.setCursor(menuX + 20 * tft.getFontWidth(), iqCorIncY);
  tft.print(plotValueInc ? 1.0 : 0.1, 1);
}

FLASHMEM void ShowIQAdjustIncrement(int adjChars) {
  int adjX;

  tft.setFontScale((enum RA8875tsize)0);
  adjX = adjChars * tft.getFontWidth();
  tft.fillRect(menuX + adjX, iqCorIncY + 15, 50, tft.getFontHeight(), RA8875_BLACK);
  tft.setTextColor(RA8875_GREEN);
  tft.setCursor(menuX + adjX, iqCorIncY +15);
  tft.print(iqCorInc, 3);
}

FLASHMEM void ShowSignalSource() {
  tft.setFontScale((enum RA8875tsize)0);
  tft.fillRect(menuX + 20 * tft.getFontWidth(), iqCorIncY + 30, 50, tft.getFontHeight(), RA8875_BLACK);
  tft.setTextColor(RA8875_GREEN);
  tft.setCursor(menuX + 20 * tft.getFontWidth(), iqCorIncY + 30);
  tft.print(signalStrengthSources[signalStrengthSource]);
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

  EncoderCenterTune();
  if(tuneChange != 0) {
    if(0) {
      // adjust image value
      plotValue += tuneChange * (plotValueInc ? 1.0 : 0.1);
      tuneChange = 0;

      tft.setFontScale((enum RA8875tsize)1);
      tft.fillRect(680, 440, 150, tft.getFontHeight(), RA8875_BLACK);
      tft.setTextColor(RA8875_GREEN);
      tft.setCursor(680, 440);
      tft.print(plotValue, 1);
    } else {
      // adjust bin size
      fftBins += tuneChange;
      if(fftBins < 5) {
        fftBins = 5;
      }
      tuneChange = 0;
    }
  }

  //if(fineTuneEncoderMove != 0) {
    //calNFAdjust -= fineTuneEncoderMove;
    //fineTuneEncoderMove = 0;
    //Serial.println(calNFAdjust);
  //}
}

FLASHMEM void PrintAtten() {
}

//-------------------------------------------------------------------------------------------------------------
// Menus and Displays
//-------------------------------------------------------------------------------------------------------------

FLASHMEM void ShowCalibrationMenu() {
  int menuY = iqCorIncY - 30;

  tft.setFontScale((enum RA8875tsize)0);
  tft.fillRect(menuX, iqCorIncY - 30, 800 - menuX, 14 * tft.getFontHeight(), RA8875_BLACK);
  tft.setTextColor(RA8875_YELLOW);
  tft.setCursor(menuX, menuY);
  tft.print("Band+-: Change band");
  menuY += 15;
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
  //tft.setCursor(menuX, menuY);
  //tft.print("15: Attn In/Out toggle");
  //menuY += 15;
  //tft.setCursor(menuX, menuY);
  //tft.print("16: Toggle directions");
  //menuY += 15;
  tft.setCursor(menuX, menuY);
  tft.print("17: Cancel");
  menuY += 15;
  tft.setCursor(menuX, menuY);
  tft.print("Select: Save/Exit");
  menuY += 15;
  //tft.setCursor(menuX, menuY);
  //tft.print("Band+/-: Change band");
  //menuY += 15;
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
  //tft.print("Fine: Atten adj");
  tft.print("Fine: NF adj");
  menuY += 15;
  tft.setCursor(menuX, menuY);
  tft.print("Center: FFT bin <>");
  //tft.print("Center: Sig str adj");
  //menuY += 15;
  //tft.setCursor(menuX, menuY);

  // display menu item values
  ShowBand();
  ShowPlotIncrement();
  ShowIQAdjustIncrement(20);
  ShowSignalSource();
}

FLASHMEM void ShowCalibrationDisplay() {
  ClearScreen();

  // display instructions
  tft.setFontScale((enum RA8875tsize)1);
  tft.setTextColor(RA8875_WHITE);
  tft.setCursor(menuX, 10);
  if(transmitCal) {
    tft.print("TX IQ Cal");
  } else {
    tft.print("RX IQ Cal");
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

FLASHMEM void PlotSpectrum(int *calBins, int binSize) {
  int yPlot, y1Plot = 0;
  static int yOldPlot[SPECTRUM_RES];
  int x, y;
  int nf = calNFAdjust;

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
    if(x < 0) {
      x = 0;
    }
    if(y > SPECTRUM_RES - 1) {
      y = SPECTRUM_RES - 1;
    }
    for(int x1 = x; x1 < y; x1++) {
    //for(int x1 = 0; x1 < SPECTRUM_RES - 1; x1++) {
    //for(int x1 = 200; x1 < 300; x1++) {

      // calculate the freq spectrum plot value; pixelnew spectrum is calculated in CalcZoomFreqSpec
      //yPlot = minPointsY - pixelnew[x1]; // - currentNF;
      //y1Plot = minPointsY - pixelnew[x1 + 1]; // - currentNF;
      yPlot = minPointsY - 10 - pixelnew[x1] + nf;
      y1Plot = minPointsY - 10 - pixelnew[x1 + 1] + nf;

      // erase the old spectrum
      tft.drawLine(SPECTRUM_LEFT_X + x1, yOldPlot[x1 + 1], SPECTRUM_LEFT_X + x1, yOldPlot[x1], RA8875_BLACK);
      //tft.drawFastVLine(SPECTRUM_LEFT_X + x1, yOldPlot[x1 + 1], yOldPlot[x1], RA8875_BLACK);

      // prevent drawing spectrum outside of the spectrum area
      if(yPlot > minPointsY - 10) {
        yPlot = minPointsY - 10;
      }
      if(y1Plot > minPointsY - 10) {
        y1Plot = minPointsY - 10;
      }
      if(yPlot < 0) {
        yPlot = 0;
      }
      if(y1Plot < 0) {
        y1Plot = 0;
      }

      // draw the new spectrum
      tft.drawLine(SPECTRUM_LEFT_X + x1, y1Plot, SPECTRUM_LEFT_X + x1, yPlot, RA8875_YELLOW);
      //tft.drawFastVLine(SPECTRUM_LEFT_X + x1, y1Plot, yPlot, RA8875_YELLOW);

      // save plot value to erase spectrum next loop
      yOldPlot[x1] = yPlot;

      PrepareExciterIQDataCal(1);
      YieldToProcess();
    }
    yOldPlot[y - 1] = y1Plot;
  }
  //yOldPlot[SPECTRUM_RES - 1] = y1Plot;
}


static float minGainX = 0.0, minGainY = 80.0;
static float minPhaseX = 0.0, minPhaseY = 80.0;

  // plotType: type of adjustment: false = phase; true = amplitude
FLASHMEM void PlotIQGainValue(float sigStr, bool plotType = true, bool dbm = false) {
  char msg[60], f1[10], f2[10];
  int color;
  float plotX, plotY, plotValue;
  float amp, phase;

  if(transmitCal) {
    amp = IQXAmpCorrectionFactor[currentBand];
    phase = IQXPhaseCorrectionFactor[currentBand];
  } else {
    amp = IQAmpCorrectionFactor[currentBand];
    phase = IQPhaseCorrectionFactor[currentBand];
  }

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
  plotY = map((int)(10.0 * plotValue), 0, SIG_STRENGTH_MAX * 10, 200 + 4.0 * 57.5, 200);
  if(plotType) {
    plotX = map(amp, GAIN_COARSE_MIN, GAIN_COARSE_MAX, 40, 440);
    color = RA8875_YELLOW;

    if(plotValue < minGainY) {
      minGainX = amp;
      minGainY = plotValue;
    }

    dtostrf(minGainX, 5, 3, f1);
    dtostrf(minGainY, 3, 1, f2);

    tft.setFontScale((enum RA8875tsize)0);
    tft.fillRect(15 * tft.getFontWidth(), minPointsY, 20 * tft.getFontWidth(), tft.getFontHeight(), RA8875_BLACK);
    tft.setTextColor(RA8875_YELLOW);
    sprintf(msg, " Gain = %.5s @  %.3s", f1, f2);
    tft.setCursor(15 * tft.getFontWidth(), minPointsY);
  } else {
    plotX = map(phase, PHASE_COARSE_MIN, PHASE_COARSE_MAX, 40, 440);
    color = RA8875_CYAN;

    if(plotValue < minPhaseY) {
      minPhaseX = phase;
      minPhaseY = plotValue;
    }

    dtostrf(minPhaseX, 6, 3, f1);
    dtostrf(minPhaseY, 3, 1, f2);

    tft.setFontScale((enum RA8875tsize)0);
    tft.fillRect(37 * tft.getFontWidth(), minPointsY, 22 * tft.getFontWidth(), tft.getFontHeight(), RA8875_BLACK);
    tft.setTextColor(RA8875_CYAN);
    if(minPhaseX < 0) {
      sprintf(msg, " Phase = %.6s @  %.3s", f1, f2);
    } else {
      sprintf(msg, " Phase = %.5s @  %.3s", f1, f2);
    }
    tft.setCursor(37 * tft.getFontWidth(), minPointsY);
  }
  tft.fillCircle(plotX, plotY, 2, color);
  tft.print(msg);
}

FLASHMEM void DrawIQGainPlot() {
  // erase only plot
  tft.fillRect(0, 185, 480, 294, RA8875_BLACK);

  // reset min value globals
  minGainX = 0.0;
  minGainY = 80.0;
  minPhaseX = 0.0;
  minPhaseY = 80.0;

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
    // x axis ticks
    tft.drawFastVLine(40 + k * 100, 430, 7, RA8875_GREEN);

    // x axis values
    tft.setCursor(30 + k * 100, 438);
    tft.print((float)GAIN_COARSE_MIN + ((float)(GAIN_COARSE_MAX - GAIN_COARSE_MIN)) / 4.0 * (float)k, 1);
    tft.setCursor(30 + k * 100, 438 + 15);
    tft.print((float)PHASE_COARSE_MIN + ((float)(PHASE_COARSE_MAX - PHASE_COARSE_MIN)) / 4.0 * (float)k, 1);

    // y axis values
    tft.setCursor(18, 190 + k * 57.5);
    tft.print(SIG_STRENGTH_MAX * (1 - (float)k / 4.0), 0);

    // y axis ticks
    tft.drawFastHLine(33, 200 + k * 57.5, 7, RA8875_GREEN);
  }

  tft.setCursor(0, 0);
  tft.print("Spectrum");
  tft.fillRect(10 * tft.getFontWidth(), 0, 25, tft.getFontHeight(), RA8875_BLACK);
  tft.setCursor(10 * tft.getFontWidth(), 0);
  tft.print(bands[currentBand].name);

  tft.setCursor(0, minPointsY);
  tft.print("Auto Cal Plot");
  //tft.setCursor(15 * tft.getFontWidth(), minPointsY);
  //tft.print("Min Points:");
}

//-------------------------------------------------------------------------------------------------------------
// Signal Strength
//-------------------------------------------------------------------------------------------------------------


FLASHMEM void GetSignalStrength(float *pSS, int passes = 0, bool getMeanSS = true) {
  bool result = false;
  int binCenter[2] = {0, 0}; // center FFT bin of [desired, undesired] signal
  int16_t adjAmplitude = 0;
  int16_t refAmplitude = 0;
  uint32_t index_of_max;
  int ssIndex = 0;
  int numSamples = 3;
  const int samplesMax = 20;
  float samples[samplesMax], stdev;
  int count = 0, meanCount = 0;
  float stdevLimit = 0.8;
  int meanCountLimit = 3;

  // set bin values depending on calibration type
  //  During calibration, only small areas of the frequency spectrum need to be examined or displayed.
  //  The calibration loop will be slow and unresponsive if the entire 512 wide frequency spectrum is displayed.
  //  The spectrum areas of interest are determined by the calibration type, demodulation mode, the calibration frequency shift applied and zoom level.
  //  The goal of calibration is to minimize the undesired, or adjacent, signal compared to the desired, or reference signal.
  //  All calibration modes use a 3kHz test signal and 0Hz calibration frequency shift.  *** this is different than the official software ***
  //  The center bins for the reference and adjacent signals can be calculated as follows:
  //    Transmit:
  //      Calibration is done at the 4x zoom scale giving a FFT bin size of 192kHz/4/512 = 93.75Hz/bin.  The 3kHz test signal is located at
  //      3000/93.75 = 32 bins left and right of the center bin (256) depending on the demodulation mode.
  //    Receive:
  //      Calibration is done at the 1x zoom scale.  In 1x zoom, the frequency spectrum is shifted left by 48kHz or 512/4 = 128 bins, creating a new "center".
  //      The bin size at 1x zoom is 192kHz/1/512 = 375Hz/bin.  The 3kHz test signal is located at 3000/375 = 8 bins left or right from this new "center"
  //      depending on the demodulation mode.  The undesired signal is mirrored on the other side of the spectrum.
  //
  if(transmitCal) {
    // transmit calibration, 4x zoom
    if(bands[currentBand].demod == DEMOD_LSB) {
      binCenter[0] = 256-32;
      binCenter[1] = 256+32;
    }
    if(bands[currentBand].demod == DEMOD_USB) {
      binCenter[1] = 256-32;
      binCenter[0] = 256+32;
    }
  } else {
    // receive calibration, 1x zoom
    if(bands[currentBand].demod == DEMOD_LSB) {
      binCenter[0] = 256-128-8;
      binCenter[1] = 256+128+8;
    }
    if(bands[currentBand].demod == DEMOD_USB) {
      binCenter[0] = 256-128+8;
      binCenter[1] = 256+128-8;
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
      PlotSpectrum(binCenter, fftBins);
      count = 0;
    } else {
      PrepareExciterIQDataCal(1);
      prevUpdate = millis();
      YieldToProcess(true);
    }

    // calculate adjacent sideband signal strength relative to reference sideband
    arm_max_q15(&pixelnew[(binCenter[0] - fftBins)], fftBins * 2, &refAmplitude, &index_of_max);
    arm_max_q15(&pixelnew[(binCenter[1] - fftBins)], fftBins * 2, &adjAmplitude, &index_of_max);

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
        //if(stdev > 1.5) {
        if(stdev > stdevLimit) {
          //Serial.print(stdev); Serial.print(", ");

          // standard deviation is too high, start again
          ssIndex = 0;

          // relax stdevLimit if we're spending too much time here
          if(meanCount++ > meanCountLimit) {
            stdevLimit += 0.1;
            meanCountLimit++;
            numSamples++;
            if(numSamples > samplesMax) numSamples = samplesMax;
            Serial.println(stdevLimit);
            meanCount = 0;
          }
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
      tft.fillRect(menuX, 400, 150, tft.getFontHeight(), RA8875_BLACK);
      tft.fillRect(menuX, 440, 150, tft.getFontHeight(), RA8875_BLACK);
      tft.setCursor(menuX, 400);
      tft.print("Sig Sup");
      tft.setCursor(menuX, 440);
      tft.print("Max Sup");
      minSignalStrength = 0;
      break;
  }
}

//-------------------------------------------------------------------------------------------------------------
// Auto Calibration
//-------------------------------------------------------------------------------------------------------------

FLASHMEM bool CheckForCancel() {
  int val;
  bool result = false;

    // check for and process button input
    val = ReadSelectedPushButton();
    if(val != BOGUS_PIN_READ) {
      val = ProcessButtonPress(val);
    }

    switch(val) {
      case BEARING: // 17
        // cancel calibration
        result = true;
        break;

      default:
        break;
    }

    return result;
}

FLASHMEM bool TuneCalParameter(int indexStart, int indexEnd, float increment, float *IQCorrectionFactor) {
  bool result = false;
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
    GetSignalStrength(&meanSignalStrength); // w/o spectrum
    //GetSignalStrength(&meanSignalStrength, 1); // with spectrum

    if(meanSignalStrength < minSignalStrength) {
      minSignalStrength = meanSignalStrength;
      minIndex = index;
      //Serial.println();
      //Serial.print("min factor "); Serial.print(correctionFactor + index * increment); Serial.print(" found @: "); Serial.println(minSignalStrength);
    } else {
      //Serial.println(minSignalStrength);
    }

    PlotIQGainValue(meanSignalStrength, (IQCorrectionFactor == &IQXAmpCorrectionFactor[currentBand]) || (IQCorrectionFactor == &IQAmpCorrectionFactor[currentBand]), true);

    // update IQ correction factor for next increment
    index++;
    *IQCorrectionFactor = correctionFactor + index * increment;
    UpdateCalDisplayData();

    if(CheckForCancel()) {
      result = true;
      break;
    }
  }

  *IQCorrectionFactor = correctionFactor + minIndex * increment;
  UpdateCalDisplayData();
  return result;
}

FLASHMEM void PrintStepMsg(const char *msg) {
  tft.setFontScale((enum RA8875tsize)0);
  tft.fillRect(menuX, 280, 250, tft.getFontHeight(), RA8875_BLACK);
  tft.setTextColor(RA8875_YELLOW);
  tft.setCursor(menuX, 280);
  tft.print(msg);
  Serial.println(msg);
}

FLASHMEM bool AutoTune(float *amp, float *phase,
                float gain_coarse_max, float gain_coarse_min,
                float phase_coarse_max, float phase_coarse_min,
                int gain_steps, int phase_steps,
                int gain_fine_steps, int phase_fine_steps) {
  int gainStepsCoarseN = (int)((gain_coarse_max - gain_coarse_min) / 0.01 / 2);
  int phaseStepsCoarseN = (int)((phase_coarse_max - phase_coarse_min) / 0.01 / 2);

  *amp = 1.0;
  *phase = 0.0;

  // clear auto cal info areas
  tft.setFontScale((enum RA8875tsize)0);
  tft.fillRect(15 * tft.getFontWidth(), minPointsY, menuX - 10, tft.getFontHeight(), RA8875_BLACK); // plot mins
  tft.setFontScale((enum RA8875tsize)1);
  tft.fillRect(menuX, 240, 250, tft.getFontHeight(), RA8875_BLACK); // title
  tft.fillRect(menuX, 280, 250, tft.getFontHeight(), RA8875_BLACK); // msg
  tft.fillRect(menuX, 400, 250, tft.getFontHeight(), RA8875_BLACK); // sig sup
  tft.fillRect(menuX, 440, 250, tft.getFontHeight(), RA8875_BLACK); // max sup

  // draw auto cal info
  tft.setTextColor(RA8875_WHITE);
  tft.setCursor(menuX, 240);
  if(transmitCal) {
    tft.print("Auto Xmit Cal");
  } else {
    tft.print("Auto Receive Cal");
  }
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

  //  proceed with auto calibration with preset step size and increments (defines at top of file)
  // Step 1: gain in 0.01 steps
  PrintStepMsg("  1. Adjusting course gain...");
  if(TuneCalParameter(-gainStepsCoarseN, gainStepsCoarseN + 1, 0.01, amp)) return false;

  // Step 2: phase in 0.01 steps
  PrintStepMsg("  2. Adjusting course phase...");
  if(TuneCalParameter(-phaseStepsCoarseN, phaseStepsCoarseN + 1, 0.01, phase)) return false;

  // Step 3: gain in 0.005 steps
  PrintStepMsg("  3. Adjusting gain...");
  if(TuneCalParameter(-gain_steps, gain_steps + 1, 0.005, amp)) return false;

  // Step 4: phase in 0.005 steps
  PrintStepMsg("  4. Adjusting phase...");
  if(TuneCalParameter(-phase_steps, phase_steps + 1, 0.005, phase)) return false;

  // Step 5: gain in 0.001 steps
  PrintStepMsg("  5. Adjusting fine gain...");
  if(TuneCalParameter(-gain_fine_steps, gain_fine_steps + 1, 0.001, amp)) return false;

  // Step 6: phase in 0.001 steps
  PrintStepMsg("  6. Adjusting fine phase...");
  if(TuneCalParameter(-phase_fine_steps, phase_fine_steps + 1, 0.001, phase)) return false;

  return true;
}
FLASHMEM void AutoCal() {
  bool result = true;

  if(transmitCal) {
    // transmit calibration
    result = AutoTune(&IQXAmpCorrectionFactor[currentBand], &IQXPhaseCorrectionFactor[currentBand],
              GAIN_COARSE_MAX, GAIN_COARSE_MIN,
              PHASE_COARSE_MAX, PHASE_COARSE_MIN,
              GAIN_STEPS, PHASE_STEPS,
              GAIN_FINE_STEPS, PHASE_FINE_STEPS);
  } else {
    // receive calibration
    result = AutoTune(&IQAmpCorrectionFactor[currentBand], &IQPhaseCorrectionFactor[currentBand],
              GAIN_COARSE_MAX, GAIN_COARSE_MIN,
              PHASE_COARSE_MAX, PHASE_COARSE_MIN,
              GAIN_STEPS, PHASE_STEPS,
              GAIN_FINE_STEPS, PHASE_FINE_STEPS);
  }

  tft.setFontScale((enum RA8875tsize)0);
  tft.fillRect(menuX, 280, 250, 2 * tft.getFontHeight(), RA8875_BLACK);
  tft.setTextColor(RA8875_WHITE);
  tft.setCursor(menuX, 280);
  if(result) {
    tft.print("  Auto Cal Done");
  } else {
    tft.print("  Auto Cal cancelled");
  }
  UpdateIQDisplay();
}


//-------------------------------------------------------------------------------------------------------------
// Frequency Calibration
//-------------------------------------------------------------------------------------------------------------



//-------------------------------------------------------------------------------------------------------------
// IQ Calibration
//-------------------------------------------------------------------------------------------------------------

// allow radio to stabilize for ms milliseconds
void StabilizeSignal(unsigned long ms) {
  unsigned long prevMillis;

  prevMillis = millis();
  while(millis() - prevMillis < ms) {
    GetSignalStrength(&signalStrength, 1, false);
    UpdateCalDisplayData();
  }
}

/*****
  Purpose: Combined input/output to calibrate the transmit or receive IQ signals
 *****/
FLASHMEM void CalibrateIQ(bool calType) {
  int calFlag = 1; // 1 = do calibration, 0 = done
  int val, bandCalBand;
  float amp, phase;

  transmitCal = calType;

  // set up for calibration
  if(transmitCal) {
    CalibratePreamble(2, CALIBRATE_TRANSMIT_STATE, CALIBRATE_TRANSMIT_STATE);
  } else {
    //CalibratePreamble(1, CW_TRANSMIT_STRAIGHT_STATE, CALIBRATE_RECEIVE_STATE);
    CalibratePreamble(1, CALIBRATE_TRANSMIT_STATE, CALIBRATE_TRANSMIT_STATE);
  }

  signalStrengthSource = 2; // use loopback signal strength source
  signalStrengthReceived = false;

  ShowCalibrationDisplay();

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
        if(currentBand == BAND_10M) ChangeBand(1); // *** skip 10m for now *** TODO: without this test signal dies passing through 10m band.  why? ***
        TxRxFreq = centerFreq = bands[currentBand].calFreq;
        //SetTxRxFreq(centerFreq);
        SetFreqCal(0);
        //si5351.set_freq((centerFreq + calFreqOffset) * SI5351_FREQ_MULT, SI5351_CLK2);
        UpdateIQDisplay();

        // clear spectrum area
        tft.fillRect(0, 15, 512, minPointsY-20, RA8875_BLACK);

        ShowBand();
        break;

      case BAND_DN: // 3
        ChangeBand(-1);
        if(currentBand == BAND_10M) ChangeBand(-1); // *** skip 10m for now *** TODO: without this test signal dies passing through 10m band.  why? ***
        TxRxFreq = centerFreq = bands[currentBand].calFreq;
        //SetTxRxFreq(centerFreq);
        SetFreqCal(0);
        //si5351.set_freq((centerFreq + calFreqOffset) * SI5351_FREQ_MULT, SI5351_CLK2);
        UpdateIQDisplay();

        // clear spectrum area
        tft.fillRect(0, 15, 512, minPointsY-20, RA8875_BLACK);

        ShowBand();
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

        ShowPlotIncrement();
        break;

      case NOISE_FLOOR: // 11
        // toggle IQ calibration increment
        iqIncrementIndex++;
        if(iqIncrementIndex >= 3) iqIncrementIndex = 0;
        iqCorInc = iqIncrementValues[iqIncrementIndex];

        ShowIQAdjustIncrement(20);
        break;

      case FINE_TUNE_INCREMENT: // 12
        // toggle signal strength source
        signalStrengthSource++;
        if(signalStrengthSource > 2) signalStrengthSource = 0;

        ShowSignalSource();

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
        //tft.fillRect(menuX, iqCorIncY - 30, 250, 14 * tft.getFontHeight(), RA8875_BLACK);
        tft.fillRect(menuX, 0, 250, 20 * tft.getFontHeight(), RA8875_BLACK);
        tft.setTextColor(RA8875_WHITE);
        tft.setCursor(menuX, 10);
        tft.print("All Bands Auto Cal Factors");
        tft.setCursor(menuX, 25);
        //         80M   1.003  0.000
        tft.print("Band  Gain   Phase");

        if(transmitCal) {
          Serial.println("All Bands Auto Transmit IQ Calibration Factors");
        } else {
          Serial.println("All Bands Auto Recieve IQ Calibration Factors");
        }
        Serial.println("Band\tGain\tPhase");

        // save current band and set to 80m band here and on external T41
        // *** this code assumes external T41 starts on 40m band ***
        bandCalBand = currentBand;
        ChangeBand(BAND_80M - currentBand);
        //SendSetBandChange(-1); // v12 external

        // cycle through bands doing auto cal
        for(int i = BAND_80M; i < NUMBER_OF_BANDS; i++) {
          // clear previous plot
          DrawIQGainPlot();

          if(bands[currentBand].calFreq > 0) {
            TxRxFreq = centerFreq = bands[currentBand].calFreq;
            //SetTxRxFreq(centerFreq);
            SetFreqCal(0);
            //si5351.set_freq((centerFreq + calFreqOffset) * SI5351_FREQ_MULT, SI5351_CLK2); // v12
            UpdateIQDisplay();

            // set up signal strength source
            SetupSignalStrengthSource(signalStrengthSource);

            // allow new band to stabilize for 5 sec
            StabilizeSignal(5000);

            // auto calibrate this band
            AutoCal();

            if(transmitCal) {
              amp = IQXAmpCorrectionFactor[currentBand];
              phase = IQXPhaseCorrectionFactor[currentBand];
            } else {
              amp = IQAmpCorrectionFactor[currentBand];
              phase = IQPhaseCorrectionFactor[currentBand];
            }

            // print factors to display and serial
            tft.setFontScale((enum RA8875tsize)0);
            tft.setTextColor(RA8875_WHITE);
            tft.setCursor(menuX, 10 + 15 * (i + 2));
            tft.print(bands[currentBand].name); tft.print("   "); tft.print(amp, 3); tft.print("  "); tft.println(phase, 3);

            Serial.print(bands[currentBand].name); Serial.print("\t"); Serial.print(amp, 3); Serial.print("\t"); Serial.println(phase, 3);
          }

          ChangeBand(1);
          //SendSetBandChange(1); // v12 external
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

        // clear previous plot
        DrawIQGainPlot();

          // set up signal strength source
        SetupSignalStrengthSource(signalStrengthSource);

            // begin auto calibration for current band
        AutoCal();

        // display auto cal results for 5 sec
        StabilizeSignal(5000);

        // redraw menu
        ShowCalibrationMenu();
        UpdateIQDisplay();
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
