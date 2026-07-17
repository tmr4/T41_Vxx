// generic calibration routines

//#include <LinearRegression.h>      // https://github.com/cubiwan/Regressino/
#include <Linear2DRegression.hpp>  // https://github.com/nkaaf/Arduino-Regression

#include "SDT.h"

#include "ButtonProc.h"
#include "calibrate.h"
#include "Demod.h"
#include "Display.h"
#include "Exciter.h"
#include "Process.h"
#include "Tune.h"
#include "t41Property.h"

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

#define SPECTRUM_RES          512

int calibrateItem = -1;
int calibrationType = -1;

// preserve/restore radio state
int userRadioState, userRadioMode, userDemodMode, userDisplayState;
int userActiveBand, userTxPower;
int userCenterFreq, userNCOFreq, userFilterHiCut, userFilterLoCut;
int userFreqSpecScale, userSpectrumZoom, userCenterTuneIndex, userAudioVolume;
float userIQAmpFactor, userIQPhaseFactor;

float32_t sinBuffer3[256];
float32_t cosBuffer3[256];

bool transmitCal; // calibration mode: true=transmit, false=receive

int fftBins = 5;  // the number of FFT bins to examine on either side of binCenter for the signal peak

int signalStrengthSource = 2; // signal strength source: 0 = manual user entry, 1 = external via CAT SM command, 2 = internal loopback
float minSignalStrength;
float signalStrength;

static float minGainX = 0.0, minGainY = 80.0;
static float minPhaseX = 0.0, minPhaseY = 80.0;

static int iqIndex = 0;
static int typeIndex = 0; // 0: course, 1: full
static int speedIndex = 0; // 0: slow, 1: med, 2: fast


//-------------------------------------------------------------------------------------------------------------
// Forwards
//-------------------------------------------------------------------------------------------------------------

void SetBPFBand(int currentBand);

void CalibrateIQBoth();

void UpdateCalDisplayData();
void CalibrateFrequency(bool reset = false);

void AutoCal();
void StabilizeSignal(unsigned long ms);
void DrawIQGainPlot();
void UpdateIQDisplay(bool autoFlag = false);

//-------------------------------------------------------------------------------------------------------------
// Code
//-------------------------------------------------------------------------------------------------------------

//-------------------------------------------------------------------------------------------------------------
// Initialization and Restoration
//-------------------------------------------------------------------------------------------------------------

/*****
save radio state
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
  userCenterTuneIndex = t41.CenterTuneIndex;
  userAudioVolume = t41.AudioVolume;
}

/*****
restore radio state after IQ calibrations
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
  t41.CenterTuneIndex = userCenterTuneIndex;
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
exit calibration mode
  *** TODO: validate this covers all state variables for all calibration types ***
 *****/
FLASHMEM void CalibrationExit() {
  t41.CalState = NOT_CAL_STATE;
  calibrationType = -1;

  ClearScreen();
  RestoreRadioState();

  // Restore the user's zoom setting
  //SetZoom(userSpectrumZoom); // ... and zoom display

  //SetFreq(t41.CenterFreq);

  // reset frequency spectrum buffers
  //InitFFTArrays();

  digitalWrite(RXTX, LOW);  // Turn off the transmitter.

  //t41.DisplayState = DISPLAY_T41;

  SetInfoBoxWindow(0);

  // restore screen
  RedrawDisplayScreen();
}

/*****
  Configure radio for selected calibration type

  *** TODO: radioState is occasionally be referenced by various global routines used
      during calibration. Verify radioState is set properly for each calibration type. ***

  Parameter List:
    calType - type of calibration we're cleaning up after
    rState  - radioState for calibration
    aState  - audio configuration state for calibration
 *****/
//FLASHMEM void CalibrationSetup(int calType, int rState, int aState) {
FLASHMEM void CalibrationSetup(int calType) {
  switch(calType) {
    case 0:  // Freq
      t41.CalState = FREQ_CAL_STATE;
      // force calibration state change (switch to WWV frequencies)
      ChangeBand(-1, false);
      ChangeBand(1, false);
      ChangeMode(DSB_MODE, DEMOD_SAM);
      t41.CenterTuneIndex = t41.GetMaxFreqIncIndex();
      CalibrateFrequency(true);
      SetInfoBoxWindow(3);
      break;

    case 1:  // Course RX/TX IQ
      break;

    case 2:  // RX IQ
      SetFreqCal(0);
      userIQAmpFactor = IQAmpCorrectionFactor[t41.ActiveBand];
      userIQPhaseFactor = IQPhaseCorrectionFactor[t41.ActiveBand];
      digitalWrite(RXTX, HIGH);  // Turn on transmitter.
      break;

    case 3:  // TX IQ
      SetFreqCal(0);
      userIQAmpFactor = IQXAmpCorrectionFactor[t41.ActiveBand];
      userIQPhaseFactor = IQXPhaseCorrectionFactor[t41.ActiveBand];
      digitalWrite(RXTX, HIGH);  // Turn on transmitter.
      break;

    case 4: // Two Tone
      break;

    case 5: // CW Pwr
      break;

    case 6: // SSB Pwr
      break;

    default:
      break;
  }

  // general calibration configuration
  //radioState = rState;
  //ConfigAudioState(aState);
}

/*****
  Perform common calibration initialization tasks
  *** TODO: validate this covers all state variables for all calibration types ***
 *****/
FLASHMEM void CalibrationInit(int calType) {
  SaveRadioState();
  calibrationType = calType;
  CalibrationSetup(calType);
}

FLASHMEM void CalibrationLoop() {
  switch(calibrationType) {
    case 0:  // Freq
      CalibrateFrequency();
      break;

    case 1:  // Course RX/TX IQ
      break;

    case 2:  // RX IQ
      break;

    case 3:  // TX IQ
      break;

    case 4: // Two Tone
      break;

    case 5: // CW Pwr
      break;

    case 6: // SSB Pwr
      break;

    default:
      break;
  }
}

FLASHMEM void CalibrationReset() {
  switch(calibrationType) {
    case 0:  // Freq
      CalibrateFrequency(true);
      break;

    case 1:  // Course RX/TX IQ
      break;

    case 2:  // RX IQ
      break;

    case 3:  // TX IQ
      break;

    case 4: // Two Tone
      break;

    case 5: // CW Pwr
      break;

    case 6: // SSB Pwr
      break;

    default:
      break;
  }
}

//-------------------------------------------------------------------------------------------------------------
// Calibration Routines
//-------------------------------------------------------------------------------------------------------------

//-------------------------------------------------------------------------------------------------------------
// Frequency Calibration
//-------------------------------------------------------------------------------------------------------------

EXTMEM Linear2DRegression corrFacReg;
/*****
  Determine frequency correction factor for SI5351

  *** can exit early but must let process complete to change global correction factor ***
 *****/
FLASHMEM void CalibrateFrequency(bool reset /* = false */) {
  int correctionFactor = 0;

  static bool startFlag = true;
  static int freqAutoIncrementSet = 0;
  static int freqCalFactor = 0;
  static int freqCalFactorStart = 0;
  static int count = 0;
  static int autoCount = 0;
  static long last = millis();

  if(reset) {
    freqCalFactor = 0;
    SetSI5351FreqCorFactor(freqCalFactor);
    startFlag = true;
    count = 0;
    last = millis();
    return;
  }

  // allow radio to stabilize between calibration steps
  if(millis() - last >= 5000) {
    if(startFlag) {
      // start auto frequency calibration
      Serial.printf("\nPerforming Frequency Calibration\nCurrent factor: %d\n", freqCalFactor);
      Serial.println("Factor\tError\tNew factor");

      corrFacReg.reset();

      // set first auto calc point
      freqCalFactorStart = freqCalFactor -(int)(GetSAMFreqError() * 100.0); // parts per billion
      autoCount = 0;
      freqAutoIncrementSet = 4000 / (1 << count);

      freqCalFactor = freqCalFactorStart + freqAutoIncrementSet * (autoCount - 5);
      SetSI5351FreqCorFactor(freqCalFactor);
      last = millis();
      startFlag = false;
    } else {
      // update correction factor regression and display
      corrFacReg.addPoint(GetSAMFreqError(), freqCalFactor);
      correctionFactor = corrFacReg.calculate(0);
      Serial.printf("%d\t%d\t%d\n", freqCalFactor, (int)(GetSAMFreqError()), correctionFactor);
      //UpdateInfoBoxItem(-6);
      autoCount++; // increment auto plot counter
      freqCalFactor = freqCalFactorStart + freqAutoIncrementSet * (autoCount - 5);

      SetSI5351FreqCorFactor(freqCalFactor);
      last = millis();

      if(autoCount >= 11) {
        SetSI5351FreqCorFactor(correctionFactor);

        Serial.printf("\nNew factor: %d\n", correctionFactor);
        freqCalFactor = correctionFactor;
        startFlag = true;

        count++;
        if(count > 5) {
          // auto mode complete, clean up
          count = 0;
          freqCorrectionFactor = correctionFactor; // change global factor
          CalibrationExit();
        }
      }
    }
  }
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

FLASHMEM void PlotSpectrum(int *calBins, int binSize) {
  //int yPlot = 0, y1Plot = 0;
  //static int yOldPlot[SPECTRUM_RES];
  int x, y;
  //int nf = calNFAdjust;

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
      //yPlot = minPointsY - 10 - pixelnew[x1] + nf;
      //y1Plot = minPointsY - 10 - pixelnew[x1 + 1] + nf;

      // erase the old spectrum
      //tft.drawLine(SPECTRUM_LEFT_X + x1, yOldPlot[x1 + 1], SPECTRUM_LEFT_X + x1, yOldPlot[x1], RA8875_BLACK);

      // prevent drawing spectrum outside of the spectrum area
      //if(yPlot > minPointsY - 10) {
      //  yPlot = minPointsY - 10;
      //}
      //if(y1Plot > minPointsY - 10) {
      //  y1Plot = minPointsY - 10;
      //}
      //if(yPlot < 0) {
      //  yPlot = 0;
      //}
      //if(y1Plot < 0) {
      //  y1Plot = 0;
      //}

      // draw the new spectrum
      //tft.drawLine(SPECTRUM_LEFT_X + x1, y1Plot, SPECTRUM_LEFT_X + x1, yPlot, RA8875_YELLOW);

      // save plot value to erase spectrum next loop
      //yOldPlot[x1] = yPlot;

      PrepareExciterIQDataCal(1);
      YieldToProcess();
    }
    //yOldPlot[y - 1] = y1Plot;
  }
  //yOldPlot[SPECTRUM_RES - 1] = y1Plot;
}

//-------------------------------------------------------------------------------------------------------------
// Signal Strength
//-------------------------------------------------------------------------------------------------------------

FLASHMEM void SetupSignalStrengthSource(int source) {
  //unsigned long prevMillis;

  // set up signal strength source
  switch(source) {
    case 0: // manual
      // not supported
      break;

    case 1: // external
      // set up this and external unit for calibration
      minSignalStrength = 0;
      signalStrengthSource = 1;
      //SendCommand(t41.CenterFreq + t41.IntermediateFreq, T41_ITEM_FREQ);
      //if(t41.DemodMode == DEMOD_LSB) {
      //  SendCommand(DEMOD_USB, T41_ITEM_DEMOD_MODE);
      //} else {
      //  SendCommand(DEMOD_LSB, T41_ITEM_DEMOD_MODE);
      //}
      //SendCommand(2, T41_ITEM_ZOOM);
      //SendNarrowFilter();

      // allow frequency to stabilize
      //prevMillis = millis();
      //while(millis() - prevMillis < 5000) {
      //  PrepareMicExciterData();
      //  T41ControlLoop();
      //}

      //UpdateCalDisplayData();
      break;

    case 2: // loopback
    default:
      minSignalStrength = 0;
      break;
  }
}

FLASHMEM void GetSignalStrength(float *pSS, int passes = 0, bool getMeanSS = true) {
  bool result = false;
  int binCenter[2] = {0, 0}; // center FFT bin of [desired, undesired] signal
  int16_t adjAmplitude = 0;
  int16_t refAmplitude = 0;
  //uint32_t index_of_max;
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
    if(t41.DemodMode == DEMOD_LSB) {
      binCenter[0] = 256-32;
      binCenter[1] = 256+32;
    }
    if(t41.DemodMode == DEMOD_USB) {
      binCenter[1] = 256-32;
      binCenter[0] = 256+32;
    }
  } else {
    // receive calibration, 1x zoom
    if(t41.DemodMode == DEMOD_LSB) {
      binCenter[0] = 256-128-8;
      binCenter[1] = 256+128+8;
    }
    if(t41.DemodMode == DEMOD_USB) {
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
    //arm_max_q15(&pixelnew[(binCenter[0] - fftBins)], fftBins * 2, &refAmplitude, &index_of_max);
    //arm_max_q15(&pixelnew[(binCenter[1] - fftBins)], fftBins * 2, &adjAmplitude, &index_of_max);

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

//-------------------------------------------------------------------------------------------------------------
// IQ Calibration
//-------------------------------------------------------------------------------------------------------------

/*****
  Combined input/output to course calibrate the transmit or receive IQ signals for all bands
 *****/
FLASHMEM void CalibrateIQAllBands() {
  int bandCalBand;
  float amp, phase;

  // auto calibrate all bands, starting with the first band

  // setup serial tables
  Serial.println("All Bands Auto Cal Factors");

  if(transmitCal) {
    Serial.println("All Bands Auto Transmit IQ Calibration Factors");
  } else {
    Serial.println("All Bands Auto Recieve IQ Calibration Factors");
  }
  Serial.println("Band\tGain\tPhase");

  // save current band and set to 80m band here and on external T41
  // *** this code assumes external T41 starts on 40m band ***
  bandCalBand = t41.ActiveBand;
  ChangeBand(BAND_80M - t41.ActiveBand);
  //SendBandChange(-1); // v12 external

  // cycle through bands doing auto cal
  for(int i = BAND_80M; i < NUMBER_OF_BANDS; i++) {
    // clear previous plot
    DrawIQGainPlot();

    if(bands[t41.ActiveBand].calFreq > 0) {
      t41.CenterFreq = bands[t41.ActiveBand].calFreq;
      //SetFreq(t41.CenterFreq);
      SetFreqCal(0);
      //si5351.set_freq((t41.CenterFreq + calFreqOffset) * SI5351_FREQ_MULT, SI5351_CLK2); // v12
      UpdateIQDisplay();

      // set up signal strength source
      SetupSignalStrengthSource(signalStrengthSource);

      // allow new band to stabilize for 5 sec
      StabilizeSignal(5000);

      // auto calibrate this band
      AutoCal();

      if(transmitCal) {
        amp = IQXAmpCorrectionFactor[t41.ActiveBand];
        phase = IQXPhaseCorrectionFactor[t41.ActiveBand];
      } else {
        amp = IQAmpCorrectionFactor[t41.ActiveBand];
        phase = IQPhaseCorrectionFactor[t41.ActiveBand];
      }

      // print factors
      Serial.print(bands[t41.ActiveBand].name); Serial.print("\t"); Serial.print(amp, 3); Serial.print("\t"); Serial.println(phase, 3);
    }

    ChangeBand(1);
    //SendBandChange(1); // v12 external
  }

  Serial.println();

  // return to original band
  ChangeBand(bandCalBand - t41.ActiveBand);
  t41.CenterFreq = bands[t41.ActiveBand].calFreq;
  SetFreq(t41.CenterFreq);
  //si5351.set_freq((t41.CenterFreq + calFreqOffset) * SI5351_FREQ_MULT, SI5351_CLK2); // v12
  UpdateIQDisplay();
}

/*****
  Combined input/output to course calibrate the transmit and receive IQ signals
 *****/
FLASHMEM void CalibrateIQBoth() {
  // calibrate receive first
  transmitCal = false;
  //CalibrationSetup(1, CALIBRATE_TRANSMIT_STATE, CALIBRATE_TRANSMIT_STATE);
  CalibrationSetup(1);

  signalStrengthSource = 2; // use loopback signal strength source
  //signalStrengthReceived = false;

  //ShowIQCalDisplay();

  // clear previous plot
  DrawIQGainPlot();

    // set up signal strength source
  SetupSignalStrengthSource(signalStrengthSource);

  StabilizeSignal(5000);

  CalibrateIQAllBands();

  // display auto cal results for 5 sec
  StabilizeSignal(5000);

  //RestoreRadioState();

  // calibrate transmit next
  transmitCal = true;
  //CalibrationSetup(2, CALIBRATE_TRANSMIT_STATE, CALIBRATE_TRANSMIT_STATE);
  CalibrationSetup(2);

  //ShowIQCalDisplay();

  // clear previous plot
  DrawIQGainPlot();

    // set up signal strength source
  SetupSignalStrengthSource(signalStrengthSource);

  StabilizeSignal(5000);

  CalibrateIQAllBands();

  StabilizeSignal(5000);

  //RestoreRadioState();
}

// plotType: type of adjustment: false = phase; true = amplitude
FLASHMEM void PlotIQGainValue(float sigStr, bool plotType = true) {
  char msg[60], f1[10], f2[10];
  //float plotX, plotY;
  float plotValue;
  float amp, phase;

  if(transmitCal) {
    amp = IQXAmpCorrectionFactor[t41.ActiveBand];
    phase = IQXPhaseCorrectionFactor[t41.ActiveBand];
  } else {
    amp = IQAmpCorrectionFactor[t41.ActiveBand];
    phase = IQPhaseCorrectionFactor[t41.ActiveBand];
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
  //plotY = map((int)(10.0 * plotValue), 0, SIG_STRENGTH_MAX * 10, 200 + 4.0 * 57.5, 200);
  if(plotType) {
    //plotX = map(amp, GAIN_COARSE_MIN, GAIN_COARSE_MAX, 40, 440);

    if(plotValue < minGainY) {
      minGainX = amp;
      minGainY = plotValue;
    }

    dtostrf(minGainX, 5, 3, f1);
    dtostrf(minGainY, 3, 1, f2);

    sprintf(msg, " Gain = %.5s @  %.3s", f1, f2);
  } else {
    //plotX = map(phase, PHASE_COARSE_MIN, PHASE_COARSE_MAX, 40, 440);

    if(plotValue < minPhaseY) {
      minPhaseX = phase;
      minPhaseY = plotValue;
    }

    dtostrf(minPhaseX, 6, 3, f1);
    dtostrf(minPhaseY, 3, 1, f2);

    if(minPhaseX < 0) {
      sprintf(msg, " Phase = %.6s @  %.3s", f1, f2);
    } else {
      sprintf(msg, " Phase = %.5s @  %.3s", f1, f2);
    }
  }
  Serial.println(msg);
}

FLASHMEM void DrawIQGainPlot() {
  // reset min value globals
  minGainX = 0.0;
  minGainY = 80.0;
  minPhaseX = 0.0;
  minPhaseY = 80.0;
}

FLASHMEM void UpdateIQDisplay(bool autoFlag /* = false */) {
  if(transmitCal) {
    Serial.println(IQXAmpCorrectionFactor[t41.ActiveBand], 3);
  } else {
    Serial.println(IQAmpCorrectionFactor[t41.ActiveBand], 3);
  }
  if(transmitCal) {
    Serial.println(IQXPhaseCorrectionFactor[t41.ActiveBand], 3);
  } else {
    Serial.println(IQPhaseCorrectionFactor[t41.ActiveBand], 3);
  }
}

FLASHMEM void UpdateCalDisplayData() {
  Serial.println(signalStrength, 1);
  Serial.println(minSignalStrength, 1);

  UpdateIQDisplay(true);
}

// allow radio to stabilize for ms milliseconds
FLASHMEM void StabilizeSignal(unsigned long ms) {
  unsigned long prevMillis;

  prevMillis = millis();
  while(millis() - prevMillis < ms) {
    GetSignalStrength(&signalStrength, 1, false);
    UpdateCalDisplayData();
  }
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
  //signalStrengthReceivedIndex = -1;
  //signalStrengthReceived = false;
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

    PlotIQGainValue(meanSignalStrength, (IQCorrectionFactor == &IQXAmpCorrectionFactor[t41.ActiveBand]) || (IQCorrectionFactor == &IQAmpCorrectionFactor[t41.ActiveBand]));

    // update IQ correction factor for next increment
    index++;
    *IQCorrectionFactor = correctionFactor + index * increment;
    UpdateCalDisplayData();

    //if(ProcessAutoCalMenu()) {
    //  result = true;
    //  break;
    //}
  }

  *IQCorrectionFactor = correctionFactor + minIndex * increment;
  UpdateCalDisplayData();
  return result;
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

  if(transmitCal) {
    Serial.println("Auto Xmit Cal: ");
  } else {
    Serial.println("Auto Receive Cal: ");
  }
  Serial.println("  starting auto cal...");

  UpdateCalDisplayData();

  //  proceed with auto calibration with preset step size and increments (defines at top of file)
  if(iqIndex == 1) {
    // Step 1: phase in 0.01 steps
    Serial.println("  1. Adjusting course phase...");
    if(TuneCalParameter(-phaseStepsCoarseN, phaseStepsCoarseN + 1, 0.01, phase)) return false;

    // Step 2: gain in 0.01 steps
    Serial.println("  2. Adjusting course gain...");
    if(TuneCalParameter(-gainStepsCoarseN, gainStepsCoarseN + 1, 0.01, amp)) return false;
  } else {
    // Step 1: gain in 0.01 steps
    Serial.println("  1. Adjusting course gain...");
    if(TuneCalParameter(-gainStepsCoarseN, gainStepsCoarseN + 1, 0.01, amp)) return false;

    // Step 2: phase in 0.01 steps
    Serial.println("  2. Adjusting course phase...");
    if(TuneCalParameter(-phaseStepsCoarseN, phaseStepsCoarseN + 1, 0.01, phase)) return false;
  }

  if(typeIndex == 0) return true; // course calibration complete

  // Step 3: gain in 0.005 steps
  Serial.println("  3. Adjusting gain...");
  if(TuneCalParameter(-gain_steps, gain_steps + 1, 0.005, amp)) return false;

  // Step 4: phase in 0.005 steps
  Serial.println("  4. Adjusting phase...");
  if(TuneCalParameter(-phase_steps, phase_steps + 1, 0.005, phase)) return false;

  // Step 5: gain in 0.001 steps
  Serial.println("  5. Adjusting fine gain...");
  if(TuneCalParameter(-gain_fine_steps, gain_fine_steps + 1, 0.001, amp)) return false;

  // Step 6: phase in 0.001 steps
  Serial.println("  6. Adjusting fine phase...");
  if(TuneCalParameter(-phase_fine_steps, phase_fine_steps + 1, 0.001, phase)) return false;

  return true;
}

FLASHMEM void AutoCal() {
  bool result = true;

  // clear previous plot
  DrawIQGainPlot();

  // set up signal strength source
  // *** TODO: verify proper place for this with auto cal ***
  SetupSignalStrengthSource(signalStrengthSource);

  if(transmitCal) {
    // transmit calibration
    result = AutoTune(&IQXAmpCorrectionFactor[t41.ActiveBand], &IQXPhaseCorrectionFactor[t41.ActiveBand]);
  } else {
    // receive calibration
    result = AutoTune(&IQAmpCorrectionFactor[t41.ActiveBand], &IQPhaseCorrectionFactor[t41.ActiveBand]);
  }

  if(result) {
    Serial.println("  Auto Cal Done");
  } else {
    Serial.println("  Auto Cal cancelled");
  }

  // *** TODO: verify correct IQ factors are displayed at end in all cases ***
  UpdateIQDisplay();

  // display auto cal results for 5 sec
  StabilizeSignal(5000);

  UpdateIQDisplay();
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
