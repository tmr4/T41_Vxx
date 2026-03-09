// v11 specific calibration file

#include "..\SDT.h"

#include <Wire.h>
#include <Adafruit_MCP23X17.h>

#include "..\AudioConfig.h"
#include "..\Button.h"
#include "..\ButtonProc.h"
#include "..\CW_Excite.h"
#include "..\Display.h"
#include "..\EEPROM.h"
#include "..\Encoders.h"
#include "..\Exciter.h"
#include "..\FIR.h"
#include "..\keyer.h"
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
float adjIncrementValues[] = { 0.001, 0.01, 0.1, 1.0 };
int adjIncrementIndex = 1;
float adjIncrement = adjIncrementValues[adjIncrementIndex];
const int menuPosX = 530;
const int menuPosY = 45;
int minPointsY = 165;
int calNFAdjust = 25;
int fftBins = 5;  // the number of FFT bins to examine on either side of binCenter for the signal peak

float minSignalStrength;
bool transmitCal; // calibration mode: true=transmit, false=receive

static int calID = 0;
static int modeIndex = 0;
static int typeIndex = 0;
static int speedIndex = 0;
static int spectrumIndex = 0;
static int iqIndex = 0;
static int autoIndex = 0;

static int pwrIndex = 0;
static int pwrTypeIndex = 0;
static int bpfIndex = 0;

const char *iqModes[] = { "receive", "transmit", "both" };
const char *iqTypes[] = { "course", "full" };
const char *iqSpeeds[] = { "slow", "med", "fast" };
const char *spectrumOptions[] = { "auto", "on", "off", "full" };
const char *iqOptions[] = { "gain", "phase" };
//const char *autoOptions[] = { "1 band", "all bands", "reset 1", "reset all" };
const char *autoOptions[] = { "current band", "all bands", "reset current", "reset all" };

const char *pwrModes[] = { "CW", "SSB", "FT8", "Two Tone" };
const char *pwrTypes[] = { "cal factor", "pwr eqn" };
const char *bpfModes[] = { "active", "bypass" };

// frequency calibration

// receive calibration

// transmit calibration
float plotValue = 0;
bool plotValueInc = true; // true = 1.0, false = 0.1
int signalStrengthSource = 2; // signal strength source: 0 = manual user entry, 1 = external via CAT SM command, 2 = internal loopback
const char *signalStrengthSources[3] =  {"man", "ext", "loop"};

// two tone variables
uint16_t GPAB_state;
#define BPF_BOARD_MCP23017_ADDR 0x20   // For BPF #0 Address

// Define BPF Band words
// Word definition: GPB7 GPB6 ... GPB0 GPA7 GPA6 ... GPA0
#define BPF_BAND_BYPASS 0x0008
#define BPF_BAND_40M    0x0800

static Adafruit_MCP23X17 mcpBPF;


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
void ChangeCalMode(int mode);
void PrepareSpectrumArea();
void ShowAutoCalTitle();
void CalibrateIQAllBands();





FLASHMEM void SetupBPF() {
  // Set Wire2 I2C bus to 100KHz and start
  Wire2.setClock(100000UL);
  Wire2.begin();

  while (!mcpBPF.begin_I2C(BPF_BOARD_MCP23017_ADDR,&Wire2)){
    Serial.println("BPF MCP23017 not found at 0x"+String(BPF_BOARD_MCP23017_ADDR,HEX));
    delay(5000);
  }

  Serial.println("BPF connected");

  // Enable the address pins A0, A1, and A2.
  mcpBPF.enableAddrPins();
  // Set all chip pins to be outputs
  for (int i=0;i<16;i++){
    mcpBPF.pinMode(i, OUTPUT);
  }

  // Set to 40m band
  GPAB_state = BPF_BAND_40M;
  //GPAB_state = BPF_BAND_BYPASS;
  mcpBPF.writeGPIOAB(GPAB_state);
}



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
  // calibration specific configuration
  switch(calType) {
    case 0: // frequency cal
      break;

    case 1: // receive IQ cal
      PrepareSpectrumArea();
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
      PrepareSpectrumArea();
      SetFreqCal(0);
      SetZoom(2); // 4x
      //SetZoom(3); // 8x
      //SetZoom(1); // 2x
      userIQAmpFactor = IQXAmpCorrectionFactor[currentBand];
      userIQPhaseFactor = IQXPhaseCorrectionFactor[currentBand];

      digitalWrite(RXTX, HIGH);  // Turn on transmitter.
      break;

    case 3: // pwr cal
      TxRxFreq = centerFreq = bands[currentBand].calFreq;
      SetFreq();  // Update frequencies if the radio state has changed
      break;

    case 4: // two tone test
      break;

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

FLASHMEM void UpdateMenuItem(int row, const char *msg) {
  int yOffset;

  tft.setFontScale((enum RA8875tsize)0);
  yOffset = menuPosY + (row - 1) * tft.getFontHeight();
  tft.fillRect(menuPosX + 20 * tft.getFontWidth(), yOffset, 105, tft.getFontHeight(), RA8875_BLACK);
  tft.setTextColor(RA8875_GREEN);
  tft.setCursor(menuPosX + 20 * tft.getFontWidth(), yOffset);
  tft.print(msg);
}

FLASHMEM void ShowValue(int row, float value, int digits) {
  int adjX;
  int yOffset;

  tft.setFontScale((enum RA8875tsize)0);
  yOffset = menuPosY + (row - 1) * tft.getFontHeight();
  adjX = 20 * tft.getFontWidth();
  tft.fillRect(menuPosX + adjX, yOffset, 50, tft.getFontHeight(), RA8875_BLACK);
  tft.setTextColor(RA8875_GREEN);
  tft.setCursor(menuPosX + adjX, yOffset);
  tft.print(value, digits);
}

FLASHMEM void ShowAdjustIncrement(int row) {
  ShowValue(row, adjIncrement, 3);
}

FLASHMEM void ShowBand() {
  UpdateMenuItem(1, bands[currentBand].name);
}

FLASHMEM void ShowIQCalMode() {
  UpdateMenuItem(2, iqModes[modeIndex]);
}

FLASHMEM void ShowCalType() {
  UpdateMenuItem(3, iqTypes[typeIndex]);
}

FLASHMEM void ShowCalSpeed() {
  UpdateMenuItem(4, iqSpeeds[speedIndex]);
}

FLASHMEM void ShowSpectrumOption() {
  UpdateMenuItem(5, spectrumOptions[spectrumIndex]);
}

FLASHMEM void ShowIQOption() {
  UpdateMenuItem(6, iqOptions[iqIndex]);
}

FLASHMEM void ShowSignalSource() {
  UpdateMenuItem(7, signalStrengthSources[signalStrengthSource]);
}

// ShowAdjustIncrement for IQ cal is row 8

FLASHMEM void ShowAutoCalMode() {
  UpdateMenuItem(9, autoOptions[autoIndex]);
}

FLASHMEM void ShowPlotIncrement() {
  const int yOffset = 0;

  tft.setFontScale((enum RA8875tsize)0);
  tft.fillRect(menuPosX + 20 * tft.getFontWidth(), yOffset, 100, tft.getFontHeight(), RA8875_BLACK);
  tft.setTextColor(RA8875_GREEN);
  tft.setCursor(menuPosX + 20 * tft.getFontWidth(), yOffset);
  tft.print(plotValueInc ? 1.0 : 0.1, 1);
}

FLASHMEM void ShowPwrCalMode() {
  UpdateMenuItem(2, pwrModes[pwrIndex]);
}

FLASHMEM void ShowPwrCalType() {
  UpdateMenuItem(3, pwrTypes[pwrTypeIndex]);
}

FLASHMEM void ShowBPF() {
  UpdateMenuItem(5, bpfModes[bpfIndex]);
}

FLASHMEM void ShowPwrFactor() {
  float pwr;

  if(pwrTypeIndex == 1) {
    pwr = CWPowerEqnCalFactor[currentBand];
  } else {
    switch(pwrIndex) {
      case 0: // CW
        pwr = CWPowerCalibrationFactor[currentBand];
        break;

      case 1: // SSB
        pwr = SSBPowerCalibrationFactor[currentBand];
        break;

      case 2: // FT8
        pwr = FT8PowerCalibrationFactor[currentBand];
        break;

      case 3: // two-tone
        pwr = CWPowerCalibrationFactor[currentBand];
        break;

      default:
        pwr = 0.0;
        break;
    }
  }

  ShowValue(9, pwr, 3);
}

FLASHMEM void UpdatePwrFactor(float factor) {
  float pwr;

  if(pwrTypeIndex == 1) {
    pwr = CWPowerEqnCalFactor[currentBand] += factor;
  } else {
    switch(pwrIndex) {
      case 0: // CW
        pwr = CWPowerCalibrationFactor[currentBand] += factor;
        break;

      case 1: // SSB
        pwr = SSBPowerCalibrationFactor[currentBand] += factor;
        break;

      case 2: // FT8
        pwr = FT8PowerCalibrationFactor[currentBand] += factor;
        break;

      case 3: // two-tone
        pwr = CWPowerCalibrationFactor[currentBand] += factor;
        break;

      default:
        pwr = 0.0;
        break;
    }
  }

  ShowValue(9, pwr, 3);
}

FLASHMEM void ShowPwr() {
  ShowValue(10, transmitPowerLevel, 0);
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
      IQXAmpCorrectionFactor[currentBand] += menuEncoderMove * adjIncrement;
    } else {
      IQAmpCorrectionFactor[currentBand] += menuEncoderMove * adjIncrement;
    }

    menuEncoderMove = 0;
    adjustFlag = true;
  }

  // IQ phase correction factor
  if(adjustVolEncoder != 0) {
    if(transmitCal) {
      IQXPhaseCorrectionFactor[currentBand] += adjustVolEncoder * adjIncrement;
    } else {
      IQPhaseCorrectionFactor[currentBand] += adjustVolEncoder * adjIncrement;
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

FLASHMEM bool AdjustPwrFactors() {
  bool adjustFlag = false;

  // power calibration factor
  if(menuEncoderMove != 0) {
    UpdatePwrFactor(menuEncoderMove * adjIncrement);
    menuEncoderMove = 0;
    adjustFlag = true;
  }

  // transmit power
  if(adjustVolEncoder != 0) {
    transmitPowerLevel += adjustVolEncoder;

    adjustVolEncoder = 0;
    if(transmitPowerLevel > 20) transmitPowerLevel = 20;
    if(transmitPowerLevel < 1) transmitPowerLevel = 1;
    ShowPwr();
    adjustFlag = true;
  }
  return adjustFlag;
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
  sprintf(msg, "Spectrum - Band: %.4s  Cal Mode: %.10s, %.7s, %.5s", bands[currentBand].name, iqModes[modeIndex], iqTypes[typeIndex], iqSpeeds[speedIndex]);
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

FLASHMEM void ShowIQCalMenu() {
  int menuY = menuPosY;
  int height;

  tft.setFontScale((enum RA8875tsize)1);
  tft.setTextColor(RA8875_WHITE);
  tft.setCursor(menuPosX, 10);
  tft.print("IQ Cal");

  tft.setFontScale((enum RA8875tsize)0);
  tft.fillRect(menuPosX, menuPosY, 800 - menuPosX, 290 - 1, RA8875_BLACK);
  tft.setTextColor(RA8875_YELLOW);
  height = tft.getFontHeight();

  tft.setCursor(menuPosX, menuY);
  tft.print("Band+-: Change band");
  menuY += height;
  tft.setCursor(menuPosX, menuY);
  tft.print("1: Cal mode");
  menuY += height;
  tft.setCursor(menuPosX, menuY);
  tft.print("3: Type");
  menuY += height;
  tft.setCursor(menuPosX, menuY);
  tft.print("4: Speed");
  menuY += height;
  tft.setCursor(menuPosX, menuY);
  tft.print("6: Spectrum");
  menuY += height;
  tft.setCursor(menuPosX, menuY);
  tft.print("7: First IQ");
  menuY += height;
  //tft.setCursor(menuPosX, menuY);
  //tft.print("9: Plot value");
  //menuY += height;
  //tft.setCursor(menuPosX, menuY);
  //tft.print("10: Plot val inc");
  //menuY += height;
  tft.setCursor(menuPosX, menuY);
  tft.print("10: Sig Source");
  menuY += height;
  tft.setCursor(menuPosX, menuY);
  tft.print("11: IQ inc");
  menuY += height;
  tft.setCursor(menuPosX, menuY);
  tft.print("13: Auto cal mode");
  menuY += height;
  tft.setCursor(menuPosX, menuY);
  tft.print("14: Auto cal");
  menuY += height;
  //tft.setCursor(menuPosX, menuY);
  //tft.print("15: Attn In/Out toggle");
  //menuY += height;
  //tft.setCursor(menuPosX, menuY);
  //tft.print("16: Toggle directions");
  //menuY += height;
  tft.setCursor(menuPosX, menuY);
  tft.print("17: Cancel");
  menuY += height;
  tft.setCursor(menuPosX, menuY);
  tft.print("Select: Save/Exit");
  menuY += height;
  //tft.setCursor(menuPosX, menuY);
  //tft.print("Band+/-: Change band");
  menuY += height;
  tft.setCursor(menuPosX, menuY);
  tft.print("Encoders:");
  menuY += height;
  tft.setCursor(menuPosX, menuY);
  tft.print("Filter: Gain adj");
  menuY += height;
  tft.setCursor(menuPosX, menuY);
  tft.print("Vol: Phase adj");
  menuY += height;
  tft.setCursor(menuPosX, menuY);
  //tft.print("Fine: Atten adj");
  tft.print("Fine: NF adj");
  menuY += height;
  tft.setCursor(menuPosX, menuY);
  tft.print("Center: FFT bin <>");
  //tft.print("Center: Sig str adj");
  //menuY += height;
  //tft.setCursor(menuPosX, menuY);

  // display menu item values
  ShowBand();
  ShowIQCalMode();
  ShowCalType();
  ShowCalSpeed();
  ShowSpectrumOption();
  ShowIQOption();
  ShowSignalSource();
  ShowAdjustIncrement(8);
  ShowAutoCalMode();
  //ShowPlotIncrement();
}

FLASHMEM void ShowPwrCalMenu() {
  int menuY = menuPosY;
  int height;

  ClearScreen();

  tft.setFontScale((enum RA8875tsize)1);
  tft.setTextColor(RA8875_WHITE);
  tft.setCursor(menuPosX, 10);
  tft.print("Pwr Cal");

  tft.setFontScale((enum RA8875tsize)0);
  tft.fillRect(menuPosX, menuPosY, 800 - menuPosX, 290 - 1, RA8875_BLACK);
  tft.setTextColor(RA8875_YELLOW);
  height = tft.getFontHeight();

  tft.setCursor(menuPosX, menuY);
  tft.print("Band+-: Change band");
  menuY += height;
  tft.setCursor(menuPosX, menuY);
  tft.print("1: Cal mode");
  menuY += height;
  tft.setCursor(menuPosX, menuY);
  tft.print("3: Type");
  menuY += height;
  tft.setCursor(menuPosX, menuY);
  tft.print("4: Factor inc");
  menuY += height;
  tft.setCursor(menuPosX, menuY);
  tft.print("8: BPF");
  menuY += height;
  tft.setCursor(menuPosX, menuY);
  tft.print("17: Cancel");
  menuY += height;
  tft.setCursor(menuPosX, menuY);
  tft.print("Select: Save/Exit");
  menuY += height;
  menuY += height;
  tft.setCursor(menuPosX, menuY);
  tft.print("Encoders:");
  menuY += height;
  tft.setCursor(menuPosX, menuY);
  tft.print("Filter: Factor adj");
  menuY += height;
  tft.setCursor(menuPosX, menuY);
  tft.print("Vol: Xmit pwr");
  //menuY += height;
  //tft.setCursor(menuPosX, menuY);

  // display menu item values
  ShowBand();
  ShowPwrCalMode();
  ShowPwrCalType();
  ShowAdjustIncrement(4);
  ShowPwrFactor();
  ShowPwr();
  ShowBPF();
}

FLASHMEM void ShowIQCalDisplay() {
  ClearScreen();

  ShowIQCalMenu();

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

FLASHMEM bool ProcessIQMenu() {
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
      modeIndex++;
      if(modeIndex > 2) modeIndex = 0;
      ShowIQCalMode();
      ChangeCalMode(modeIndex);
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
      typeIndex++;
      if(typeIndex > 1) typeIndex = 0;
      ShowCalType();
      ShowSpectrumTitle();
      break;

    case MAIN_MENU_DN: // 4
      // change calibration speed
      speedIndex++;
      if(speedIndex > 2) speedIndex = 0;
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
      iqIndex++;
      if(iqIndex > 1) iqIndex = 0;
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
      adjIncrementIndex++;
      if(adjIncrementIndex > 2) adjIncrementIndex = 0;
      adjIncrement = adjIncrementValues[adjIncrementIndex];

      ShowAdjustIncrement(8);
      break;

    case FINE_TUNE_INCREMENT: // 12
      break;

    case DECODER_TOGGLE: // 13
      // toggle auto calibration options
      autoIndex++;
      if(autoIndex > 3) autoIndex = 0;
      ShowAutoCalMode();
      break;

    case MAIN_TUNE_INCREMENT: // 14
      // auto calibrate per auto cal option
      switch(autoIndex) {
        case 0:
          // begin auto calibration for current band and cal mod
          // *** receive cal is setup if cal mode = both ***
          AutoCal();

          // perform transmit cal if cal mode = both
          if(modeIndex == 2) {
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
          if((modeIndex == 0) || (modeIndex == 2)) {
            ResetReceiveIQFactors();
          }
          if((modeIndex == 1) || (modeIndex == 2)) {
            ResetTransmitIQFactors();
          }
          UpdateIQDisplay();
          break;

        case 3:
          // reset IQ factors for all bands
          for(int i = 0; i < NUMBER_OF_BANDS; i++) {
            if((modeIndex == 0) || (modeIndex == 2)) {
              ResetReceiveIQFactors(i);
            }
            if((modeIndex == 1) || (modeIndex == 2)) {
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

FLASHMEM bool ProcessPwrMenu() {
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
      Serial.println("select received");
      //calFlag = 0;
      break;

    case MAIN_MENU_UP: // 1
      // change calibration mode
      pwrIndex++;
      if(pwrIndex > 3) pwrIndex = 0;
      ShowPwrCalMode();
      break;

    case BAND_UP: // 2
      ChangeBand(1);
      if(currentBand == BAND_10M) ChangeBand(1); // *** skip 10m for now *** TODO: without this test signal dies passing through 10m band.  why? ***
      TxRxFreq = centerFreq = bands[currentBand].calFreq;
      SetFreqCal(0);
      ShowBand();
      break;

    case ZOOM: // 3
      // change calibration type
      pwrTypeIndex++;
      if(pwrTypeIndex > 1) pwrTypeIndex = 0;
      if(pwrTypeIndex == 1) {
        pwrScale = false;
      } else {
        pwrScale = true;
      }
      ShowPwrCalType();
      ShowPwrFactor();
      break;

    case MAIN_MENU_DN: // 4
      // change adjustment inc
      adjIncrementIndex++;
      if(adjIncrementIndex > 3) adjIncrementIndex = 0;
      adjIncrement = adjIncrementValues[adjIncrementIndex];
      ShowAdjustIncrement(4);
      break;

    case BAND_DN: // 5
      ChangeBand(-1);
      if(currentBand == BAND_10M) ChangeBand(-1); // *** skip 10m for now *** TODO: without this test signal dies passing through 10m band.  why? ***
      TxRxFreq = centerFreq = bands[currentBand].calFreq;
      SetFreqCal(0);
      ShowBand();
      break;

    case SET_MODE: // 8
      // change BPF mode
      bpfIndex++;
      if(bpfIndex > 1) bpfIndex = 0;
      ShowBPF();
      if(bpfIndex == 1) {
        // bypass BPF
        GPAB_state = BPF_BAND_BYPASS;
      } else {
        // set BPF to 40m band
        GPAB_state = BPF_BAND_40M;
      }
      mcpBPF.writeGPIOAB(GPAB_state);
      break;

    case BEARING: // 17
      // cancel calibration
      Serial.println("cancel received");
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
      arm_scale_f32(sinBuffer3, 0.01, audioBufferL_EX, 256);
      arm_scale_f32(cosBuffer3, 0.01, audioBufferR_EX, 256);
      //arm_scale_f32(sinBuffer3, 0.005, audioBufferL_EX, 256);
      //arm_scale_f32(cosBuffer3, 0.005, audioBufferR_EX, 256);
      //arm_scale_f32(sinBuffer3, 0.001, audioBufferL_EX, 256);
      //arm_scale_f32(cosBuffer3, 0.001, audioBufferR_EX, 256);

      // use same scaling factor as for CW
      // *** TODO: why use a different buffer and scaling factor? compare CW vs SSB exciter chain ***
      // scaled to give 1W output when CWPowerCalibrationFactor = 1.0
      // output pwr measured with AD3 (Exp dB ave weight 100 for 500 samples) on -30dB tap of 20W dummy load
      //arm_scale_f32(sinBuffer2, 0.03385, audioBufferL_EX, 256);
      //arm_scale_f32(cosBuffer2, 0.03385, audioBufferR_EX, 256);


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

  switch(speedIndex) {
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
        typeIndex++;
        if(typeIndex > 1) typeIndex = 0;
        ShowAutoCalTitle();
        break;

      case MAIN_MENU_DN: // 4
        // change calibration speed
        speedIndex++;
        if(speedIndex > 2) speedIndex = 0;
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
  if(iqIndex == 1) {
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

  if(typeIndex == 0) return true; // course calibration complete

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
  ShowIQCalMenu();
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

  SetupBPF();

  calID = 0;

  CalibrationInit();

  ShowIQCalDisplay();

  ChangeCalMode(modeIndex);

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

    calFlag = ProcessIQMenu();
  }
}

FLASHMEM void ChangeCalMode(int mode) {
  int calType = 0;

  switch(calID) {
    case 1: // pwr cal
      calType = 3;
      break;

    default:
      break;
  }

  //Serial.println(displayState);
  switch(calType + mode) {
    // IQ cal: case 0-2
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

    // pwr cal: case 3-5

    case 3 ... 5: // CW, SSB, FT8
      radioMode = CW_MODE;
      //CalibrationSetup(3, CALIBRATE_TRANSMIT_STATE, CALIBRATE_TRANSMIT_STATE);
      //CalibrationSetup(3, CALIBRATE_TRANSMIT_STATE, CW_TRANSMIT_STRAIGHT_STATE);
      CalibrationSetup(3, CW_RECEIVE_STATE, CW_RECEIVE_STATE);
      break;

    default:
      break;
  }
  //Serial.println(displayState);
}

//-------------------------------------------------------------------------------------------------------------
// Power Calibration
//-------------------------------------------------------------------------------------------------------------

float twoToneScaler = 1.0;

FLASHMEM void AdjustTwoToneScaler() {
  EncoderCenterTune();
  if(tuneChange != 0) {
    twoToneScaler += tuneChange * adjIncrement;
    tuneChange = 0;
    Serial.println(twoToneScaler*1000);
  }
}

FLASHMEM void PrepareTwoToneData() {
  static unsigned long long time = 0;
  //const int x = 3;
  //const float w1 = 2.0 * PI * 700.0 / 24000.0;
  //const float w2 = 2.0 * PI * 1900.0 / 24000.0;
  const float w1 = 2.0 * PI * 700.0 / 192000.0;
  const float w2 = 2.0 * PI * 1900.0 / 192000.0;
  //const float w1 = 2.0 * PI * 700.0 * x / 192000.0;
  //const float w2 = 2.0 * PI * 1900.0 * x / 192000.0;

  // prepare exciter IQ buffers with two tone signal at 24000Hz sample rate
  //for(int i = 0; i < 256; i++) {
  for(int i = 0; i < 2048; i++) {
    audioBufferL_EX[i] = twoToneScaler * 0.5 * (arm_sin_f32(w1 * (float)time) + arm_sin_f32(w2 * (float)time));
    audioBufferR_EX[i] = 0.5 * (arm_cos_f32(w1 * (float)time) + arm_cos_f32(w2 * (float)time));

    // increment time and loop at sample rate
    time++;
    //if(time == 24000) time = 0;
  }
}

FLASHMEM void TwoToneTransmit() {
  double tp = transmitPowerLevel;
  double cwPwr;

  digitalWrite(RXTX, HIGH); // turn on TX relay

  // start generating CW signal
  // *** TODO: don't really care here if we press a key or PTT. Somewhere else might??? ***
  while(digitalRead(paddleDit) == LOW || digitalRead(PTT) == LOW) {
    // check for two-tone adjustment
    AdjustTwoToneScaler();
    AdjustPwrFactors();

    // prepare two-tone data
    PrepareTwoToneData();

    if(currentDemodMode == DEMOD_LSB) {
      arm_scale_f32(audioBufferL_EX, IQXAmpCorrectionFactor[currentBand], audioBufferL_EX, 2048);
      IQPhaseCorrection(audioBufferL_EX, audioBufferR_EX, IQXPhaseCorrectionFactor[currentBand], 2048);
    } else if(currentDemodMode == DEMOD_USB) {
      arm_scale_f32(audioBufferL_EX, -IQXAmpCorrectionFactor[currentBand], audioBufferL_EX, 2048);
      IQPhaseCorrection(audioBufferL_EX, audioBufferR_EX, IQXPhaseCorrectionFactor[currentBand] * 2.0, 2048);
    }

    /*
    // *** TODO: refactor some exciter routine for this ***
    // play it
    // adjust IQ signal amplitude and phase
    if(currentDemodMode == DEMOD_LSB) {
      arm_scale_f32(audioBufferL_EX, IQXAmpCorrectionFactor[currentBand], audioBufferL_EX, 256);
      IQPhaseCorrection(audioBufferL_EX, audioBufferR_EX, IQXPhaseCorrectionFactor[currentBand], 256);
    } else if(currentDemodMode == DEMOD_USB) {
      arm_scale_f32(audioBufferL_EX, -IQXAmpCorrectionFactor[currentBand], audioBufferL_EX, 256);
      IQPhaseCorrection(audioBufferL_EX, audioBufferR_EX, IQXPhaseCorrectionFactor[currentBand] * 2.0, 256);
    }

    // interpolation I channel by 2 to 48kHz
    arm_fir_interpolate_f32(&FIR_int1_EX_I, audioBufferL_EX, audioBufferTemp, 256);

    // interpolation I channel by 4 to 192 kHz
    arm_fir_interpolate_f32(&FIR_int2_EX_I, audioBufferTemp, audioBufferL_EX, 512);

    // interpolate 2x and 4x again with Q channel
    arm_fir_interpolate_f32(&FIR_int1_EX_Q, audioBufferR_EX, audioBufferTemp, 256);
    arm_fir_interpolate_f32(&FIR_int2_EX_Q, audioBufferTemp, audioBufferR_EX, 512);
    */

    // scale to compensate for losses in interpolation and output pwr
    if(pwrScale) {
      cwPwr = (6.3749 * pow(tp, 5.0) - 154.46 * pow(tp, 4.0) + 1437.3 * pow(tp, 3.0) - 6384.5 * pow(tp, 2.0) + 17189.0 * tp + 962.75) / 100000.0 * CWPowerCalibrationFactor[currentBand];
    } else {
      //cwPwr = CWPowerEqnCalFactor[currentBand] / 8.0;
      cwPwr = CWPowerEqnCalFactor[currentBand] / 4.0;
    }
    arm_scale_f32(audioBufferL_EX, cwPwr, audioBufferL_EX, 2048);
    arm_scale_f32(audioBufferR_EX, cwPwr, audioBufferR_EX, 2048);

    q15_t q15_buffer_LTemp[2048];
    q15_t q15_buffer_RTemp[2048];

    arm_float_to_q15(audioBufferL_EX, q15_buffer_LTemp, 2048);
    arm_float_to_q15(audioBufferR_EX, q15_buffer_RTemp, 2048);

    // we'll get discountinuities without this
    // *** TODO: set a default for this and return to that upon any change ***
    Q_out_L_Ex.setBehaviour(AudioPlayQueue::ORIGINAL);
    Q_out_R_Ex.setBehaviour(AudioPlayQueue::ORIGINAL);
    Q_out_L_Ex.play(q15_buffer_LTemp, 2048);
    Q_out_R_Ex.play(q15_buffer_RTemp, 2048);
    Q_out_L_Ex.setBehaviour(AudioPlayQueue::NON_STALLING);
    Q_out_R_Ex.setBehaviour(AudioPlayQueue::NON_STALLING);
  }

  digitalWrite(RXTX, LOW);

  // delay a bit to allow play buffer to empty, otherwise
  // the remaining buffer will be played next time it's connected
  CWPause(50);
}

FLASHMEM void CalibratePwr() {
  int calFlag = 1; // 1 = do calibration, 0 = done
  int audioState = radioState;

  SetupBPF();

  calID = 1;

  CalibrationInit();

  ShowPwrCalMenu();

  ChangeCalMode(pwrIndex);

  Q_out_L.setBehaviour(AudioPlayQueue::NON_STALLING); // FT8 decoding slow without this *** TODO: examine audio memory issues ***

  // calibration loop
  while(true) {
    if(calFlag == 0) {
      // calibration has finished, clean up and exit
      RestoreRadioState();
      break;
    }

    if(radioState != lastState) {
      radioState = CALIBRATE_TRANSMIT_STATE;
      ConfigAudioState(radioState);
      ShowTransmitReceiveStatus();
      ShowPwrCalMode();
      ShowPwrCalType();

      // save radio state for next loop
      lastState = radioState;
    }

    AdjustPwrFactors();

    while(digitalRead(paddleDit) == LOW || digitalRead(PTT) == LOW) {
      // prepare radio
      switch(pwrIndex) {
        case 0: // CW
          radioState = CW_TRANSMIT_STRAIGHT_STATE;
          audioState = CW_TRANSMIT_STRAIGHT_STATE;
          break;

        case 1: // SSB
          break;

        case 2: // FT8
          break;

        case 3: // two-tone
          radioState = SSB_TRANSMIT_STATE;
          audioState = CW_TRANSMIT_STRAIGHT_STATE;
          break;

        default:
          break;
      }

      ConfigAudioState(audioState);
      SetFreq();  // Update frequencies if the radio state has changed
      ShowTransmitReceiveStatus();

      // play signal
      switch(pwrIndex) {
        case 0: // CW
          CWTransmit();
          break;

        case 1: // SSB
          break;

        case 2: // FT8
          break;

        case 3: // two-tone
          TwoToneTransmit();
          break;

        default:
          break;
      }
    }

    calFlag = ProcessPwrMenu();
  }
}
