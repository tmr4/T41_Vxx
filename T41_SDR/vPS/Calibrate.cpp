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
const int menuPosX = 530;
const int menuPosY = 45;
int minPointsY = 165;
int calNFAdjust = 25;
int fftBins = 5;  // the number of FFT bins to examine on either side of binCenter for the signal peak

float minSignalStrength;
bool transmitCal; // calibration mode: true=transmit, false=receive

int calModeIndex = 0;
int calTypeIndex = 0;
int calSpeedIndex = 0;
int spectrumIndex = 0;
int calIQIndex = 0;
int autoCalIndex = 0;

const char *calModes[] = { "receive", "transmit", "both" };
const char *calTypes[] = { "course", "full" };
const char *calSpeeds[] = { "slow", "med", "fast" };
const char *calSpectrumOptions[] = { "auto", "on", "off", "full" };
const char *calIQOptions[] = { "gain", "phase" };
//const char *autoCalOptions[] = { "1 band", "all bands", "reset 1", "reset all" };
const char *autoCalOptions[] = { "current band", "all bands", "reset current", "reset all" };

// frequency calibration

// receive calibration

// transmit calibration
float plotValue = 0;
bool plotValueInc = true; // true = 1.0, false = 0.1
int signalStrengthSource = 2; // signal strength source: 0 = manual user entry, 1 = external via CAT SM command, 2 = internal loopback
const char *signalStrengthSources[3] =  {"man", "ext", "loop"};

// two tone variables


int userTransmitPowerLevel;

float32_t sinBuffer3[256];
float32_t cosBuffer3[256];

// delete when ready
int calTypeFlag = 0;

//-------------------------------------------------------------------------------------------------------------
// Forwards
//-------------------------------------------------------------------------------------------------------------

void SetFreqCal(long calFreqShift);

//void DrawIQGainPlot();
void PrepareExciterIQDataCal(int mode);
void SetupSignalStrengthSource(int source);
void AutoCal();
void StabilizeSignal(unsigned long ms);
void ChangeCalMode();
void PrepareSpectrumArea();
void ShowAutoCalTitle();
void CalibrateIQAllBands();

//-------------------------------------------------------------------------------------------------------------
// Code
//-------------------------------------------------------------------------------------------------------------

//-------------------------------------------------------------------------------------------------------------
// Initialization and Setup
//-------------------------------------------------------------------------------------------------------------

/*****
  Purpose: save radio state before IQ calibrations
  *** TODO: validate this covers all state variables for all calibration types ***
  *** TODO: consider moving to init function if not called separately anywhere ***
 *****/
FLASHMEM void SaveRadioState() {
  // Save the current operating state to restore later
  userTxRxFreq = TxRxFreq;
  userNCOFreq = NCOFreq;
  userCenterFreq = centerFreq;
  userRadioState = radioState;
  userMode = radioMode;
  userDemodMode = currentDemodMode;
  userZoomIndex = spectrumZoom;
  userBand = currentBand;
  userScale = currentScale;
  userVol = audioVolume;
  userTransmitPowerLevel = transmitPowerLevel;
}

/*****
  Purpose: perform common calibration initialization tasks
  *** TODO: validate this covers all state variables for all calibration types ***
 *****/
FLASHMEM void CalibrationInit() {
  SaveRadioState();

  displayState = DISPLAY_CALIBRATION;

  TxRxFreq = centerFreq = centerFreq + NCOFreq;
  NCOFreq = 0;

  audioVolume = 2;
  //transmitPowerLevel = 5;
  transmitPowerLevel = 1;
  //powerOutCW[currentBand] = (-.0133 * transmitPowerLevel * transmitPowerLevel + .7884 * transmitPowerLevel + 4.5146) * CWPowerCalibrationFactor[currentBand];

  for(int i = 0; i < 256; i++) {
    // used in calibration
    float theta = i * 2.0 * PI * 3000.0 / 24000.0;
    cosBuffer3[i] = cos(theta);
    sinBuffer3[i] = sin(theta);
    //cosBuffer3[i] = 0.0;
    //sinBuffer3[i] = 0.0;
  }

}

/*****
  Purpose: restore radio state after IQ calibrations
  *** TODO: validate proper restoration for all calibration types ***
 *****/
FLASHMEM void RestoreRadioState() {
  // restore radio operating state
  TxRxFreq = userTxRxFreq;
  NCOFreq = userNCOFreq;
  centerFreq = userCenterFreq;
  radioState = userRadioState;
  radioMode = userMode;
  currentDemodMode = userDemodMode;
  spectrumZoom = userZoomIndex;
  currentScale = userScale;
  volSetting = userVol;

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

  digitalWrite(RXTX, LOW);  // Turn off the transmitter.

  transmitPowerLevel = userTransmitPowerLevel;  // Restore the user's transmit power level setting.  KF5N August 15, 2023

  // restore screen
  tft.writeTo(L2);
  tft.clearMemory();
  tft.writeTo(L1);
  tft.clearMemory();

  RedrawDisplayScreen();

  displayState = DISPLAY_T41;
  //lastState = -1; // force radio state reset
  lastState = CALIBRATE_TRANSMIT_STATE; // force radio state reset
}

/*****
  Purpose: configure radio for selected calibration type

  *** TODO: radioState is occasionally be referenced by various global routines used
      during calibration. Verify radioState is set properly for each calibration type. ***

  Parameter List:
    calType - type of calibration we're cleaning up after
    rState  - radioState for calibration
    aState  - audio configuration state for calibration
 *****/
FLASHMEM void CalibrationSetup(int calType, int rState, int aState) {
  PrepareSpectrumArea();

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

      digitalWrite(RXTX, HIGH);  // Turn on transmitter.
      break;

    case 2: // transmit IQ cal
      SetFreqCal(0);
      SetZoom(2); // 4x
      //SetZoom(3); // 8x
      //SetZoom(1); // 2x
      userIQAmpFactor = IQXAmpCorrectionFactor[currentBand];
      userIQPhaseFactor = IQXPhaseCorrectionFactor[currentBand];

      digitalWrite(RXTX, HIGH);  // Turn on transmitter.
      break;

    case 3: // two tone test
    default:
      break;
  }

  //digitalWrite(MUTE, HIGH);  //  Mute Audio  (HIGH=Mute)

  // general calibration configuration
  radioState = rState;
  ConfigAudioState(aState);
  //SetFreq(); // v12
}

//-------------------------------------------------------------------------------------------------------------
// Menu Items Related
//-------------------------------------------------------------------------------------------------------------

FLASHMEM void UpdateMenuItem(int offset, const char *msg) {
  int yOffset = menuPosY + offset;

  tft.setFontScale((enum RA8875tsize)0);
  tft.fillRect(menuPosX + 20 * tft.getFontWidth(), yOffset, 105, tft.getFontHeight(), RA8875_BLACK);
  tft.setTextColor(RA8875_GREEN);
  tft.setCursor(menuPosX + 20 * tft.getFontWidth(), yOffset);
  tft.print(msg);
}

FLASHMEM void ShowBand() {
  UpdateMenuItem(0, bands[currentBand].name);
}

FLASHMEM void ShowCalMode() {
  UpdateMenuItem(15, calModes[calModeIndex]);
}

FLASHMEM void ShowCalType() {
  UpdateMenuItem(30, calTypes[calTypeIndex]);
}

FLASHMEM void ShowCalSpeed() {
  UpdateMenuItem(45, calSpeeds[calSpeedIndex]);
}

FLASHMEM void ShowSpectrumOption() {
  UpdateMenuItem(60, calSpectrumOptions[spectrumIndex]);
}

FLASHMEM void ShowIQOption() {
  UpdateMenuItem(75, calIQOptions[calIQIndex]);
}

FLASHMEM void ShowSignalSource() {
  UpdateMenuItem(90, signalStrengthSources[signalStrengthSource]);
}

FLASHMEM void ShowIQAdjustIncrement(int adjChars) {
  int adjX;
  const int yOffset = menuPosY + 105;

  tft.setFontScale((enum RA8875tsize)0);
  adjX = adjChars * tft.getFontWidth();
  tft.fillRect(menuPosX + adjX, yOffset, 50, tft.getFontHeight(), RA8875_BLACK);
  tft.setTextColor(RA8875_GREEN);
  tft.setCursor(menuPosX + adjX, yOffset);
  tft.print(iqCorInc, 3);
}

FLASHMEM void ShowAutoCalMode() {
  UpdateMenuItem(120, autoCalOptions[autoCalIndex]);
}

FLASHMEM void ShowPlotIncrement() {
  const int yOffset = 0;

  tft.setFontScale((enum RA8875tsize)0);
  tft.fillRect(menuPosX + 20 * tft.getFontWidth(), yOffset, 100, tft.getFontHeight(), RA8875_BLACK);
  tft.setTextColor(RA8875_GREEN);
  tft.setCursor(menuPosX + 20 * tft.getFontWidth(), yOffset);
  tft.print(plotValueInc ? 1.0 : 0.1, 1);
}

//-------------------------------------------------------------------------------------------------------------
// Factor Items Related
//-------------------------------------------------------------------------------------------------------------

FLASHMEM void SetIQFactors(float *amp, float *phase, float ampVal, float phaseVal) {
  *amp = ampVal;
  *phase = phaseVal;
}

FLASHMEM void ResetIQFactors(float *amp, float *phase) {
  SetIQFactors(amp, phase, 1.0, 0.0);
}

FLASHMEM void ResetReceiveIQFactors(int band) {
  ResetIQFactors(&IQAmpCorrectionFactor[band], &IQPhaseCorrectionFactor[band]);
}

FLASHMEM void ResetTransmitIQFactors(int band) {
  ResetIQFactors(&IQXAmpCorrectionFactor[band], &IQXPhaseCorrectionFactor[band]);
}

FLASHMEM void ResetReceiveIQFactors() {
  ResetReceiveIQFactors(currentBand);
}

FLASHMEM void ResetTransmitIQFactors() {
  ResetTransmitIQFactors(currentBand);
}

FLASHMEM void SetReceiveIQFactors(float ampVal, float phaseVal) {
  SetIQFactors(&IQAmpCorrectionFactor[currentBand], &IQPhaseCorrectionFactor[currentBand], ampVal, phaseVal);
}

FLASHMEM void SetTransmitIQFactors(float ampVal, float phaseVal) {
  SetIQFactors(&IQXAmpCorrectionFactor[currentBand], &IQXPhaseCorrectionFactor[currentBand], ampVal, phaseVal);
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
// Plot Routines
//-------------------------------------------------------------------------------------------------------------

FLASHMEM void ShowSpectrumTitle() {
  char msg[60];

  tft.setFontScale((enum RA8875tsize)0);
  tft.fillRect(0, 0, 512, tft.getFontHeight(), RA8875_BLACK);
  tft.setTextColor(RA8875_WHITE);
  tft.setCursor(0, 0);
  sprintf(msg, "Spectrum - Band: %.4s  Cal Mode: %.10s, %.7s, %.5s", bands[currentBand].name, calModes[calModeIndex], calTypes[calTypeIndex], calSpeeds[calSpeedIndex]);
  tft.print(msg);

}

// clear spectrum area
FLASHMEM void PrepareSpectrumArea() {
  ShowSpectrumTitle();

  tft.drawRect(0, 20, 512+2,  minPointsY - 25, RA8875_WHITE);
  tft.fillRect(1, 20+1, 512, minPointsY - 25 - 2, RA8875_BLACK);
}

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
FLASHMEM void PlotIQGainValue(float sigStr, bool plotType = true) {
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

  // convert dBm signal strength to S scale if needed
  if(signalStrengthSource > 0) {
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
  tft.drawRect(0, 185, 480, 294, RA8875_WHITE);
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

  tft.setCursor(0, minPointsY);
  tft.print("Auto Cal Plot");
  //tft.setCursor(15 * tft.getFontWidth(), minPointsY);
  //tft.print("Min Points:");
}

//-------------------------------------------------------------------------------------------------------------
// Menus and Overall Display
//-------------------------------------------------------------------------------------------------------------

FLASHMEM void ShowCalibrationMenu() {
  int menuY = menuPosY;

  tft.setFontScale((enum RA8875tsize)0);
  tft.fillRect(menuPosX, menuPosY, 800 - menuPosX, 290 - 1, RA8875_BLACK);
  tft.setTextColor(RA8875_YELLOW);

  tft.setCursor(menuPosX, menuY);
  tft.print("Band+-: Change band");
  menuY += 15;
  tft.setCursor(menuPosX, menuY);
  tft.print("1: Cal mode");
  menuY += 15;
  tft.setCursor(menuPosX, menuY);
  tft.print("3: Type");
  menuY += 15;
  tft.setCursor(menuPosX, menuY);
  tft.print("4: Speed");
  menuY += 15;
  tft.setCursor(menuPosX, menuY);
  tft.print("6: Spectrum");
  menuY += 15;
  tft.setCursor(menuPosX, menuY);
  tft.print("7: First IQ");
  menuY += 15;
  //tft.setCursor(menuPosX, menuY);
  //tft.print("9: Plot value");
  //menuY += 15;
  //tft.setCursor(menuPosX, menuY);
  //tft.print("10: Plot val inc");
  //menuY += 15;
  tft.setCursor(menuPosX, menuY);
  tft.print("10: Sig Source");
  menuY += 15;
  tft.setCursor(menuPosX, menuY);
  tft.print("11: IQ inc");
  menuY += 15;
  tft.setCursor(menuPosX, menuY);
  tft.print("13: Auto cal mode");
  menuY += 15;
  tft.setCursor(menuPosX, menuY);
  tft.print("14: Auto cal");
  menuY += 15;
  //tft.setCursor(menuPosX, menuY);
  //tft.print("15: Attn In/Out toggle");
  //menuY += 15;
  //tft.setCursor(menuPosX, menuY);
  //tft.print("16: Toggle directions");
  //menuY += 15;
  tft.setCursor(menuPosX, menuY);
  tft.print("17: Cancel");
  menuY += 15;
  tft.setCursor(menuPosX, menuY);
  tft.print("Select: Save/Exit");
  menuY += 15;
  //tft.setCursor(menuPosX, menuY);
  //tft.print("Band+/-: Change band");
  menuY += 15;
  tft.setCursor(menuPosX, menuY);
  tft.print("Encoders:");
  menuY += 15;
  tft.setCursor(menuPosX, menuY);
  tft.print("Filter: Gain adj");
  menuY += 15;
  tft.setCursor(menuPosX, menuY);
  tft.print("Vol: Phase adj");
  menuY += 15;
  tft.setCursor(menuPosX, menuY);
  //tft.print("Fine: Atten adj");
  tft.print("Fine: NF adj");
  menuY += 15;
  tft.setCursor(menuPosX, menuY);
  tft.print("Center: FFT bin <>");
  //tft.print("Center: Sig str adj");
  //menuY += 15;
  //tft.setCursor(menuPosX, menuY);

  // display menu item values
  ShowBand();
  ShowCalMode();
  ShowCalType();
  ShowCalSpeed();
  ShowSpectrumOption();
  ShowIQOption();
  ShowSignalSource();
  ShowIQAdjustIncrement(20);
  ShowAutoCalMode();
  //ShowPlotIncrement();
}

FLASHMEM void ShowIQCalDisplay() {
  ClearScreen();

  // display instructions
  tft.setFontScale((enum RA8875tsize)1);
  tft.setTextColor(RA8875_WHITE);
  tft.setCursor(menuPosX, 10);
  tft.print("IQ Cal");

  ShowCalibrationMenu();

  // display IQ factors and image value
  tft.setFontScale((enum RA8875tsize)1);
  tft.setTextColor(RA8875_WHITE);
  tft.setCursor(menuPosX, 320);
  tft.print("IQ Gain");
  tft.setCursor(menuPosX, 360);
  tft.print("IQ Phase");

  tft.setCursor(menuPosX, 400);
  tft.print("Sig Sup");
  tft.setCursor(menuPosX, 440);
  if(signalStrengthSource == 0) {
    tft.print("Plot Val");
  } else {
    tft.print("Max Sup");
  }

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

FLASHMEM bool ProcessMenu() {
  int calFlag = 1; // 1 = do calibration, 0 = done
  int val;

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

    case MAIN_MENU_UP: // 1
      // change calibration mode
      calModeIndex++;
      if(calModeIndex > 2) calModeIndex = 0;
      ShowCalMode();
      ChangeCalMode();
      ShowSpectrumTitle();
      break;

    case BAND_UP: // 2
      ChangeBand(1);
      if(currentBand == BAND_10M) ChangeBand(1); // *** skip 10m for now *** TODO: without this test signal dies passing through 10m band.  why? ***
      TxRxFreq = centerFreq = bands[currentBand].calFreq;
      //SetTxRxFreq(centerFreq);
      SetFreqCal(0);
      //si5351.set_freq((centerFreq + calFreqOffset) * SI5351_FREQ_MULT, SI5351_CLK2);
      UpdateIQDisplay();

      PrepareSpectrumArea();
      ShowBand();
      break;

    case ZOOM: // 3
      // change calibration type
      calTypeIndex++;
      if(calTypeIndex > 1) calTypeIndex = 0;
      ShowCalType();
      ShowSpectrumTitle();
      break;

    case MAIN_MENU_DN: // 4
      // change calibration speed
      calSpeedIndex++;
      if(calSpeedIndex > 2) calSpeedIndex = 0;
      ShowCalSpeed();
      ShowSpectrumTitle();
      break;

    case BAND_DN: // 5
      ChangeBand(-1);
      if(currentBand == BAND_10M) ChangeBand(-1); // *** skip 10m for now *** TODO: without this test signal dies passing through 10m band.  why? ***
      TxRxFreq = centerFreq = bands[currentBand].calFreq;
      //SetTxRxFreq(centerFreq);
      SetFreqCal(0);
      //si5351.set_freq((centerFreq + calFreqOffset) * SI5351_FREQ_MULT, SI5351_CLK2);
      UpdateIQDisplay();

      PrepareSpectrumArea();
      ShowBand();
      break;

    case FILTER: // 6
      // change spectrum option
      spectrumIndex++;
      if(spectrumIndex > 3) spectrumIndex = 0;
      ShowSpectrumOption();
      if(spectrumIndex == 2) PrepareSpectrumArea(); // spectrum is off, clear the plot area
      break;

    case DEMODULATION: // 7
      // change spectrum option
      calIQIndex++;
      if(calIQIndex > 1) calIQIndex = 0;
      ShowIQOption();
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

    /*
    case NOISE_REDUCTION:  // 9
      // plot sginal level value
      PlotIQGainValue(plotValue);
      break;

    case NOTCH_FILTER: // 10
      // toggle image level change, true = 1.0, false = 0.1
      plotValueInc = !plotValueInc;

      ShowPlotIncrement();
      break;
    */
    case NOISE_REDUCTION:  // 9
      break;

    case NOTCH_FILTER: // 10
      // toggle signal strength source
      signalStrengthSource++;
      if(signalStrengthSource > 2) signalStrengthSource = 0;

      ShowSignalSource();
      /*
      tft.setFontScale((enum RA8875tsize)1);
      tft.setTextColor(RA8875_WHITE);
      if(signalStrengthSource == 0) {
        // manual signal strength
        tft.setCursor(menuPosX, 440);
        tft.print("Plot Val");
        //tft.fillRect(680, 440, 150, tft.getFontHeight(), RA8875_BLACK);
        PrintAtten();
      } else {
        // set up signal strength source
        SetupSignalStrengthSource(signalStrengthSource);

        tft.fillRect(menuPosX, 400, 150, tft.getFontHeight(), RA8875_BLACK);
        tft.fillRect(menuPosX, 440, 150, tft.getFontHeight(), RA8875_BLACK);
        tft.setCursor(menuPosX, 400);
        tft.print("Sig Sup");
        tft.setCursor(menuPosX, 440);
        tft.print("Max Sup");

        UpdateCalDisplayData();
      }
      */
      break;

    case NOISE_FLOOR: // 11
      // toggle IQ calibration increment
      iqIncrementIndex++;
      if(iqIncrementIndex > 2) iqIncrementIndex = 0;
      iqCorInc = iqIncrementValues[iqIncrementIndex];

      ShowIQAdjustIncrement(20);
      break;

    case FINE_TUNE_INCREMENT: // 12
      break;

    case DECODER_TOGGLE: // 13
      // toggle auto calibration options
      autoCalIndex++;
      if(autoCalIndex > 3) autoCalIndex = 0;
      ShowAutoCalMode();
      break;

    case MAIN_TUNE_INCREMENT: // 14
      // auto calibrate per auto cal option
      switch(autoCalIndex) {
        case 0:
          // begin auto calibration for current band and cal mod
          // *** receive cal is setup if cal mode = both ***
          AutoCal();

          // perform transmit cal if cal mode = both
          if(calModeIndex == 2) {
            transmitCal = true;
            CalibrationSetup(2, CALIBRATE_TRANSMIT_STATE, CALIBRATE_TRANSMIT_STATE);
            AutoCal();
          }
          // *** TODO: decide whether to return to receive display since that's what both mode normally shows ***
          break;

        case 1:
          CalibrateIQAllBands();
          break;

        case 2:
          // reset IQ factors for current band
          if((calModeIndex == 0) || (calModeIndex == 2)) {
            ResetReceiveIQFactors();
          }
          if((calModeIndex == 1) || (calModeIndex == 2)) {
            ResetTransmitIQFactors();
          }
          UpdateIQDisplay();
          break;

        case 3:
          // reset IQ factors for all bands
          for(int i = 0; i < NUMBER_OF_BANDS; i++) {
            if((calModeIndex == 0) || (calModeIndex == 2)) {
              ResetReceiveIQFactors(i);
            }
            if((calModeIndex == 1) || (calModeIndex == 2)) {
              ResetTransmitIQFactors(i);
            }
          }
          UpdateIQDisplay();
          break;

        default :
          break;
      }
      break;

    case BEARING: // 17
      // cancel calibration
      if(transmitCal) {
        SetTransmitIQFactors(userIQAmpFactor, userIQPhaseFactor);
      } else {
        SetReceiveIQFactors(userIQAmpFactor, userIQPhaseFactor);
      }
      calFlag = 0;
      break;

    default:
      break;
  }

  return calFlag;
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

  switch(calSpeedIndex) {
    case 0: // slow
      numSamples = 10;
      stdevLimit = 0.5;
      meanCountLimit = 10;
      break;

    case 1: // med
      numSamples = 5;
      stdevLimit = 1.0;
      meanCountLimit = 5;
      break;

    case 2: // fast
    default:
      numSamples = 2;
      stdevLimit = 1.5;
      meanCountLimit = 3;
      break;
  }

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
    if(currentDemodMode == DEMOD_LSB) {
      binCenter[0] = 256-32;
      binCenter[1] = 256+32;
    }
    if(currentDemodMode == DEMOD_USB) {
      binCenter[1] = 256-32;
      binCenter[0] = 256+32;
    }
  } else {
    // receive calibration, 1x zoom
    if(currentDemodMode == DEMOD_LSB) {
      binCenter[0] = 256-128-8;
      binCenter[1] = 256+128+8;
    }
    if(currentDemodMode == DEMOD_USB) {
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

FLASHMEM void SetupSignalStrengthSource(int source) {
  unsigned long prevMillis;

  // set up signal strength source
  switch(source) {
    case 0: // manual
      tft.setTextColor(RA8875_WHITE);
      tft.setCursor(menuPosX, 400);
      tft.print("Sig Sup");
      tft.setCursor(menuPosX, 440);
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

      tft.fillRect(menuPosX, 400, 150, tft.getFontHeight(), RA8875_BLACK);
      tft.fillRect(menuPosX, 440, 150, tft.getFontHeight(), RA8875_BLACK);
      tft.setCursor(menuPosX, 400);
      tft.print("Sig Sup");
      tft.setCursor(menuPosX, 440);
      tft.print("Max Sup");

      UpdateCalDisplayData();
      break;

    case 2: // loopback
    default:
      tft.fillRect(menuPosX, 400, 150, tft.getFontHeight(), RA8875_BLACK);
      tft.fillRect(menuPosX, 440, 150, tft.getFontHeight(), RA8875_BLACK);
      tft.setCursor(menuPosX, 400);
      tft.print("Sig Sup");
      tft.setCursor(menuPosX, 440);
      tft.print("Max Sup");
      minSignalStrength = 0;
      break;
  }
}

//-------------------------------------------------------------------------------------------------------------
// Auto Calibration
//-------------------------------------------------------------------------------------------------------------

FLASHMEM bool ProcessAutoCalMenu() {
  int val;
  bool result = false;

    // check for and process button input
    val = ReadSelectedPushButton();
    if(val != BOGUS_PIN_READ) {
      val = ProcessButtonPress(val);
    }

    switch(val) {
      case MAIN_MENU_UP: // 1
        // change calibration type
        calTypeIndex++;
        if(calTypeIndex > 1) calTypeIndex = 0;
        ShowAutoCalTitle();
        break;

      case MAIN_MENU_DN: // 4
        // change calibration speed
        calSpeedIndex++;
        if(calSpeedIndex > 2) calSpeedIndex = 0;
        ShowAutoCalTitle();
        break;

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
    Serial.println(index);
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

    PlotIQGainValue(meanSignalStrength, (IQCorrectionFactor == &IQXAmpCorrectionFactor[currentBand]) || (IQCorrectionFactor == &IQAmpCorrectionFactor[currentBand]));

    // update IQ correction factor for next increment
    index++;
    *IQCorrectionFactor = correctionFactor + index * increment;
    UpdateCalDisplayData();

    if(ProcessAutoCalMenu()) {
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
  tft.fillRect(menuPosX, 280, 250, tft.getFontHeight(), RA8875_BLACK);
  tft.setTextColor(RA8875_YELLOW);
  tft.setCursor(menuPosX, 280);
  tft.print(msg);
  Serial.println(msg);
}

FLASHMEM void ShowAutoCalTitle() {
  tft.setFontScale((enum RA8875tsize)1);
  tft.setTextColor(RA8875_WHITE);
  tft.setCursor(menuPosX, 240);
  if(transmitCal) {
    tft.print("Auto Xmit Cal: ");
  } else {
    tft.print("Auto Receive Cal: ");
  }
}

float gain_coarse_max = GAIN_COARSE_MAX;
float gain_coarse_min = GAIN_COARSE_MIN;
float phase_coarse_max = PHASE_COARSE_MAX;
float phase_coarse_min = PHASE_COARSE_MIN;
int gain_steps = GAIN_STEPS;
int phase_steps = PHASE_STEPS;
int gain_fine_steps = GAIN_FINE_STEPS;
int phase_fine_steps = PHASE_FINE_STEPS;

FLASHMEM bool AutoTune(float *amp, float *phase) {
  int gainStepsCoarseN = (int)((gain_coarse_max - gain_coarse_min) / 0.01 / 2);
  int phaseStepsCoarseN = (int)((phase_coarse_max - phase_coarse_min) / 0.01 / 2);

  //*amp = 1.0;
  //*phase = 0.0;
  //*amp = 1.0;
  //*phase = 0.0;

  // clear auto cal info areas
  tft.setFontScale((enum RA8875tsize)0);
  tft.fillRect(15 * tft.getFontWidth(), minPointsY, menuPosX - 10, tft.getFontHeight(), RA8875_BLACK); // plot mins
  tft.setFontScale((enum RA8875tsize)1);
  tft.fillRect(menuPosX, 240, 250, tft.getFontHeight(), RA8875_BLACK); // title
  tft.fillRect(menuPosX, 280, 250, tft.getFontHeight(), RA8875_BLACK); // msg
  tft.fillRect(menuPosX, 400, 250, tft.getFontHeight(), RA8875_BLACK); // sig sup
  tft.fillRect(menuPosX, 440, 250, tft.getFontHeight(), RA8875_BLACK); // max sup

  // draw auto cal info
  ShowAutoCalTitle();
  tft.setCursor(menuPosX, 400);
  tft.setTextColor(RA8875_YELLOW);
  tft.print("Sig Sup");
  tft.setCursor(menuPosX, 440);
  tft.print("Max Sup");
  tft.setFontScale((enum RA8875tsize)0);
  tft.setCursor(menuPosX, 280);
  tft.print("  starting auto cal...");
  tft.setTextColor(RA8875_YELLOW);
  tft.setCursor(menuPosX, 295);
  tft.print("  (press 17 to cancel)");

  UpdateCalDisplayData();

  //  proceed with auto calibration with preset step size and increments (defines at top of file)
  if(calIQIndex == 1) {
    // Step 1: phase in 0.01 steps
    PrintStepMsg("  1. Adjusting course phase...");
    if(TuneCalParameter(-phaseStepsCoarseN, phaseStepsCoarseN + 1, 0.01, phase)) return false;

    // Step 2: gain in 0.01 steps
    PrintStepMsg("  2. Adjusting course gain...");
    if(TuneCalParameter(-gainStepsCoarseN, gainStepsCoarseN + 1, 0.01, amp)) return false;
  } else {
    // Step 1: gain in 0.01 steps
    PrintStepMsg("  1. Adjusting course gain...");
    if(TuneCalParameter(-gainStepsCoarseN, gainStepsCoarseN + 1, 0.01, amp)) return false;

    // Step 2: phase in 0.01 steps
    PrintStepMsg("  2. Adjusting course phase...");
    if(TuneCalParameter(-phaseStepsCoarseN, phaseStepsCoarseN + 1, 0.01, phase)) return false;
  }

  if(calTypeIndex == 0) return true; // course calibration complete

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

  // erase menu
  tft.setFontScale((enum RA8875tsize)0);
  tft.fillRect(menuPosX, menuPosY, 250, 14 * tft.getFontHeight(), RA8875_BLACK);

  // clear previous plot
  DrawIQGainPlot();

  // set up signal strength source
  // *** TODO: verify proper place for this with auto cal ***
  SetupSignalStrengthSource(signalStrengthSource);

  if(transmitCal) {
    // transmit calibration
    result = AutoTune(&IQXAmpCorrectionFactor[currentBand], &IQXPhaseCorrectionFactor[currentBand]);
  } else {
    // receive calibration
    result = AutoTune(&IQAmpCorrectionFactor[currentBand], &IQPhaseCorrectionFactor[currentBand]);
  }

  tft.setFontScale((enum RA8875tsize)0);
  tft.fillRect(menuPosX, 280, 250, 2 * tft.getFontHeight(), RA8875_BLACK);
  tft.setTextColor(RA8875_WHITE);
  tft.setCursor(menuPosX, 280);
  if(result) {
    tft.print("  Auto Cal Done");
  } else {
    tft.print("  Auto Cal cancelled");
  }

  // *** TODO: verify correct IQ factors are displayed at end in all cases ***
  UpdateIQDisplay();

  // display auto cal results for 5 sec
  StabilizeSignal(5000);

  // redraw menu
  ShowCalibrationMenu();
  UpdateIQDisplay();
}


//-------------------------------------------------------------------------------------------------------------
// Frequency Calibration
//-------------------------------------------------------------------------------------------------------------



//-------------------------------------------------------------------------------------------------------------
// IQ Calibration
//-------------------------------------------------------------------------------------------------------------

// allow radio to stabilize for ms milliseconds
FLASHMEM void StabilizeSignal(unsigned long ms) {
  unsigned long prevMillis;

  prevMillis = millis();
  while(millis() - prevMillis < ms) {
    GetSignalStrength(&signalStrength, 1, false);
    UpdateCalDisplayData();
  }
}

/*****
  Purpose: Combined input/output to course calibrate the transmit or receive IQ signals for all bands
 *****/
FLASHMEM void CalibrateIQAllBands() {
/*
  int bandCalBand;
  float amp, phase;

  // auto calibrate all bands, starting with the first band

  // setup display and serial tables
  tft.setFontScale((enum RA8875tsize)0);
  tft.fillRect(menuPosX, 0, 250, 20 * tft.getFontHeight(), RA8875_BLACK);
  tft.setTextColor(RA8875_WHITE);
  tft.setCursor(menuPosX, 10);
  tft.print("All Bands Auto Cal Factors");
  tft.setCursor(menuPosX, 25);
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
      tft.setCursor(menuPosX, 10 + 15 * (i + 2));
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
*/
}

/*****
  Purpose: Combined input/output to course calibrate the transmit and receive IQ signals
 *****/
FLASHMEM void CalibrateIQBoth() {
  // calibrate receive first
  transmitCal = false;
  CalibrationSetup(1, CALIBRATE_TRANSMIT_STATE, CALIBRATE_TRANSMIT_STATE);

  signalStrengthSource = 2; // use loopback signal strength source
  signalStrengthReceived = false;

  ShowIQCalDisplay();

  // erase menu
  tft.setFontScale((enum RA8875tsize)0);
  tft.fillRect(menuPosX, menuPosY, 250, 14 * tft.getFontHeight(), RA8875_BLACK);

        // clear previous plot
  DrawIQGainPlot();

    // set up signal strength source
  SetupSignalStrengthSource(signalStrengthSource);

  StabilizeSignal(5000);

  // begin auto calibration for current band
  //AutoCal();
  CalibrateIQAllBands();

  // display auto cal results for 5 sec
  StabilizeSignal(5000);

  RestoreRadioState();

  // calibrate transmit next
  transmitCal = true;
  calibrateItem = 3;
  CalibrationSetup(2, CALIBRATE_TRANSMIT_STATE, CALIBRATE_TRANSMIT_STATE);

  ShowIQCalDisplay();

  // erase menu
  tft.setFontScale((enum RA8875tsize)0);
  tft.fillRect(menuPosX, menuPosY, 250, 14 * tft.getFontHeight(), RA8875_BLACK);

        // clear previous plot
  DrawIQGainPlot();

    // set up signal strength source
  SetupSignalStrengthSource(signalStrengthSource);

  StabilizeSignal(5000);

  // begin auto calibration for current band
  //AutoCal();
  CalibrateIQAllBands();

  StabilizeSignal(5000);

  RestoreRadioState();
}

/*****
  Purpose: Combined input/output to calibrate the transmit or receive IQ signals
 *****/
FLASHMEM void CalibrateIQ() {
  int calFlag = 1; // 1 = do calibration, 0 = done

  CalibrationInit();

  ShowIQCalDisplay();

  ChangeCalMode();

  // calibration loop
  while(true) {
    if(calFlag == 0) {
      // calibration has finished, clean up and exit
      RestoreRadioState();
      break;
    }

    AdjustCalFactors();

    GetSignalStrength(&signalStrength, 1, false);
    UpdateCalDisplayData();

    calFlag = ProcessMenu();
  }
}

FLASHMEM void ChangeCalMode() {
  //Serial.println(displayState);
  switch(calModeIndex) {
    case 0: // receive
      transmitCal = false;
      CalibrationSetup(1, CALIBRATE_TRANSMIT_STATE, CALIBRATE_TRANSMIT_STATE);
      break;

    case 1: // transmit
      transmitCal = true;
      CalibrationSetup(2, CALIBRATE_TRANSMIT_STATE, CALIBRATE_TRANSMIT_STATE);
      break;

    case 2: // both
      // start with receive
      transmitCal = false;
      CalibrationSetup(1, CALIBRATE_TRANSMIT_STATE, CALIBRATE_TRANSMIT_STATE);
      break;

    default:
      break;
  }
  //Serial.println(displayState);
}
