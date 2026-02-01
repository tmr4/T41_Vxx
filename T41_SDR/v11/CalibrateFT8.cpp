// v11 specific calibration file

#include "..\SDT.h"

#include "..\AudioConfig.h"
#include "..\Button.h"
#include "..\ButtonProc.h"
#include "..\Display.h"
#include "..\EEPROM.h"
#include "..\Encoders.h"
#include "..\FIR.h"
#include "..\Menu.h"
#include "..\MenuProc.h"
#include "..\Process.h"
#include "..\Tune.h"
#include "..\Utility.h"

//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

static int val;
static int corrChange;
static float correctionIncrement;
static int userScale, userZoomIndex, userXmtMode;
static int transmitPowerLevelTemp;
static int calTypeFlag = 0;
static int calOnFlag = 0;
static int IQCalType;

static float32_t sinBuffer3[256];
static float32_t cosBuffer3[256];

//-------------------------------------------------------------------------------------------------------------
// Forwards
//-------------------------------------------------------------------------------------------------------------

void FT8ShowSpectrum2();
float FT8PlotCalSpectrum(int x1, int cal_bins[2], int capture_bins, int currentNF);
void CalcZoomFreqSpec(uint32_t blockSize); // needed for ProcessTransmitCalIQData
void Calc1xFreqSpec();
void FreqShift1(int blockSize);

void SetFreqCal(long calFreqShift);

//-------------------------------------------------------------------------------------------------------------
// Code
//-------------------------------------------------------------------------------------------------------------

/*****
  Purpose: Set up prior to IQ calibrations.  New function.  KF5N August 14, 2023
  These things need to be saved here and restored in the prologue function:
  Vertical scale in dB  (set to 10 dB during calibration)
  Zoom, set to 1X in receive and 4X in transmit calibrations.
  Transmitter power, set to 5W during both calibrations.
   Parameter List:
      int setZoom   (This parameter should be 0 for receive (1X) and 2 (4X) for transmit)

   Return value:
      void
 *****/
FLASHMEM void FT8CalibratePreamble(int setZoom) {
  calOnFlag = 1;
  corrChange = 0;
  correctionIncrement = 0.01;
  //correctionIncrement = 0.1;
  IQCalType = 0;
  radioState = CW_TRANSMIT_STRAIGHT_STATE;
  //radioState = CW_RECEIVE_STATE;
  //radioState = CALIBRATE_TRANSMIT_STATE;
  transmitPowerLevelTemp = transmitPowerLevel;
  transmitPowerLevel = 5;
  powerOutCW[currentBand] = (-.0133 * transmitPowerLevel * transmitPowerLevel + .7884 * transmitPowerLevel + 4.5146) * CWPowerCalibrationFactor[currentBand];
  userXmtMode = radioMode;          // Store the user's mode setting
  userZoomIndex = spectrumZoom;  // Save the zoom index so it can be reset at the conclusion
  SetZoom(setZoom);
  tft.writeTo(L2);  // Erase the bandwidth bar
  tft.clearMemory();
  tft.writeTo(L1);
  tft.setFontScale((enum RA8875tsize)0);
  tft.setTextColor(RA8875_GREEN);
  tft.setCursor(350, 160);
  tft.print("user1 - Gain/Phase");
  tft.setCursor(350, 175);
  tft.print("User2 - Incr");
  tft.setTextColor(RA8875_CYAN);
  tft.fillRect(350, 125, 100, tft.getFontHeight(), RA8875_BLACK);
  tft.fillRect(0, 272, 517, 399, RA8875_BLACK);  // Erase waterfall.  KF5N August 14, 2023
  tft.setCursor(400, 125);
  tft.print("dB");
  tft.setCursor(350, 110);
  tft.print("Incr= ");
  tft.setCursor(400, 110);
  tft.print(correctionIncrement, 3);
  userScale = currentScale;  //  Remember user preference so it can be reset when done.  KF5N
  currentScale = 1;          //  Set vertical scale to 10 dB during calibration.  KF5N
  //updateSpectrumData = false;
  digitalWrite(MUTE, LOW);  //turn off mute

  //ConfigAudioState(radioState);
  ConfigAudioState(CALIBRATE_TRANSMIT_STATE);

  centerFreq = TxRxFreq;
  NCOFreq = 0L;
  digitalWrite(MUTE, HIGH);  //  Mute Audio  (HIGH=Mute)
  digitalWrite(RXTX, HIGH);  // Turn on transmitter.
  ShowTransmitReceiveStatus();
  ShowSpectrumdBScale();

  // from v11 Utility.cpp GenSineToneBuffers
  float theta, freqSideTone3 = 3000.0;

  for(int i = 0; i < 256; i++) {
    // used in calibration
    theta = i * 2.0 * PI * freqSideTone3 / 44100.0;
    //theta = i * 2.0 * PI * freqSideTone3 / 22050.0;
    //theta = i * 2.0 * PI * freqSideTone3 / 11025.0;
    cosBuffer3[i] = cos(theta);
    sinBuffer3[i] = sin(theta);
  }
}

/*****
  Purpose: Shut down and clean up after IQ calibrations.  New function.  KF5N August 14, 2023

   Parameter List:
      void

   Return value:
      void
 *****/
FLASHMEM void FT8CalibratePrologue() {
  digitalWrite(RXTX, LOW);  // Turn off the transmitter.
  //updateSpectrumData = false;
  ShowTransmitReceiveStatus();
  // Clear queues to reduce transient.
  Q_in_L.clear();
  Q_in_R.clear();
  centerFreq = TxRxFreq;
  NCOFreq = 0L;
  currentScale = userScale;                     //  Restore vertical scale to user preference.  KF5N
  ShowSpectrumdBScale();
  radioMode = userXmtMode;   // Restore the user's floor setting.  KF5N July 27, 2023
  transmitPowerLevel = transmitPowerLevelTemp;  // Restore the user's transmit power level setting.  KF5N August 15, 2023
  EEPROMWrite();                                // Save calibration numbers and configuration.  KF5N August 12, 2023
  // Restore the user's zoom setting
  SetZoom(userZoomIndex); // ... and zoom display
  EEPROMWrite();                                // Save calibration numbers and configuration.  KF5N August 12, 2023
  tft.writeTo(L2);  // Clear layer 2.  KF5N July 31, 2023
  tft.clearMemory();
  tft.writeTo(L1);  // Exit function in layer 1.  KF5N August 3, 2023
  RedrawDisplayScreen();
  calOnFlag = 0;
  lastState = -1;  // This is required due to the function deactivating the receiver.  This forces a pass through the receiver set-up code.  KF5N October 16, 2023
  return;
}

static void UpdateIQCorrection(bool xmit = true) {
  if(xmit) {
    //  Read encoder and update values.
    if(IQCalType == 0) {
      IQXAmpCorrectionFactor[currentBand] = GetEncoderValueLive(-2.0, 2.0, IQXAmpCorrectionFactor[currentBand], correctionIncrement, (char *)"IQ Gain X");
    } else {
      IQXPhaseCorrectionFactor[currentBand] = GetEncoderValueLive(-2.0, 2.0, IQXPhaseCorrectionFactor[currentBand], correctionIncrement, (char *)"IQ Phase X");
    }
  } else {
    if(IQCalType == 0) {
      IQAmpCorrectionFactor[currentBand] = GetEncoderValueLive(-2.0, 2.0, IQAmpCorrectionFactor[currentBand], correctionIncrement, (char *)"IQ Gain");
    } else {
      IQPhaseCorrectionFactor[currentBand] = GetEncoderValueLive(-2.0, 2.0, IQPhaseCorrectionFactor[currentBand], correctionIncrement, (char *)"IQ Phase");
    }
  }
}

/*****
  Purpose: Combined input/ output for the purpose of calibrating the receive IQ

   Parameter List:
      void

   Return value:
      void
 *****/
FLASHMEM void FT8DoReceiveCalibrate() {
  int task = -1;
  int lastUsedTask = -2;
  int IQChoice = 0;
  long calFreqShift = 0;

  FT8CalibratePreamble(0);                                                   // Set zoom to 1X.
  if(bands[currentBand].demod == DEMOD_LSB) calFreqShift = 24000 - 2000;  // LSB offset
  if(bands[currentBand].demod == DEMOD_USB) calFreqShift = 24000 + 2250;  // USB offset
  SetFreqCal(calFreqShift);
  calTypeFlag = 0;  // RX cal
  // Receive calibration loop
  while(true) {
    FT8ShowSpectrum2();
    val = ReadSelectedPushButton();
    if(val != BOGUS_PIN_READ) {
      val = ProcessButtonPress(val);
      if(val != lastUsedTask && task == -100) task = val;
      else task = BOGUS_PIN_READ;
    }
    switch(task) {
        // Toggle gain and phase
      case UNUSED_1:
        IQCalType = !IQCalType;
        break;
        // Toggle increment value
      case BEARING:  // UNUSED_2 is now called BEARING
        corrChange = !corrChange;
        if(corrChange == 1) {
          correctionIncrement = 0.001;
        } else {                        //if(corrChange == 0)                   // corrChange is a toggle, so if not needed
          correctionIncrement = 0.01;
        }
        tft.setFontScale((enum RA8875tsize)0);
        tft.fillRect(400, 110, 50, tft.getFontHeight(), RA8875_BLACK);
        tft.setCursor(400, 110);
        tft.print(correctionIncrement, 3);
        break;
      case MENU_OPTION_SELECT:
        tft.fillRect(SECONDARY_MENU_X, MENUS_Y, EACH_MENU_WIDTH + 35, CHAR_HEIGHT, RA8875_BLACK);
        EEPROMData.IQAmpCorrectionFactor[currentBand] = IQAmpCorrectionFactor[currentBand];
        EEPROMData.IQPhaseCorrectionFactor[currentBand] = IQPhaseCorrectionFactor[currentBand];
        IQChoice = 6;
        break;
      default:
        break;
    }  // End switch
    if(task != -1) lastUsedTask = task;  //  Save the last used task.
    task = -100;                          // Reset task after it is used.

    // read encoder and update values
    UpdateIQCorrection(false);

    if(IQChoice == 6) break;  // Exit the while loop.
  }

  FT8CalibratePrologue();
}

/*****
  Purpose: Combined input/ output for the purpose of calibrating the transmit IQ
           calibrateItem = 3
 *****/
FLASHMEM void FT8DoXmitCalibrate() {
  int IQChoice = 0;

  RedrawDisplayScreen();

  //FT8CalibratePreamble(2);  // set zoom to 4x
  FT8CalibratePreamble(1);  // set zoom to 2x
  calTypeFlag = 1;       // TX cal
  SetFreqCal(750);
  tft.writeTo(L1);

  // Transmit Calibration Loop
  while(true) {
    FT8ShowSpectrum2();
    val = ReadSelectedPushButton();
    if(val != BOGUS_PIN_READ) {
      val = ProcessButtonPress(val);
    }
    switch(val) {
      // Toggle gain and phase
      case UNUSED_1:
        IQCalType = !IQCalType;
        break;

      // Toggle increment value
      case BEARING:  // UNUSED_2 is now called BEARING
        corrChange = !corrChange;
        if(corrChange == 1) {          // Toggle increment value
          correctionIncrement = 0.001;
        } else {
          correctionIncrement = 0.01;
        }
        tft.setFontScale((enum RA8875tsize)0);
        tft.fillRect(400, 110, 50, tft.getFontHeight(), RA8875_BLACK);
        tft.setCursor(400, 110);
        tft.print(correctionIncrement, 3);
        break;

      case (MENU_OPTION_SELECT):  // Save values and exit calibration.
        tft.fillRect(SECONDARY_MENU_X, MENUS_Y, EACH_MENU_WIDTH + 35, CHAR_HEIGHT, RA8875_BLACK);
        EEPROMData.IQXAmpCorrectionFactor[currentBand] = IQAmpCorrectionFactor[currentBand];
        EEPROMData.IQXPhaseCorrectionFactor[currentBand] = IQPhaseCorrectionFactor[currentBand];
        IQChoice = 6;
        break;

      default:
        break;
    }

    // read encoder and update values
    UpdateIQCorrection();

    if(IQChoice == 6) break;  //  Exit the while loop.
  }

  FT8CalibratePrologue();
}


/*****
  Purpose: Signal processing for the purpose of calibrating the transmit IQ

   Parameter List:
      void

   Return value:
      void
 *****/
FLASHMEM bool FT8ProcessIQData2(bool updateSpectrumData) {
  float bandCouplingFactor[7] = { 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5 };
  float bandOutputFactor;
  float rfGainValue;
  float recBandFactor[7] = { 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0 };
  static int reqPasses = 20;
  static int passes = 20;
  bool updateFreqSpec = false; // true: spectrums updated, otherwise false

  /**********************************************************************************
        Get samples from queue buffers
        Teensy Audio Library stores ADC data in two buffers size=128, Q_in_L and Q_in_R as initiated from the audio lib.
        Then the buffers are  read into two arrays sp_L and sp_R in blocks of 128 up to 2048 bytes.  The arrarys are
        of size BUFFER_SIZE*N_BLOCKS.  BUFFER_SIZE is 128, N_BLOCKS = FFT_L / 2 / BUFFER_SIZE * DF = 16 with DF = 8 and FFT_L = 512
        BUFFER_SIZE*N_BLOCKS = 2048 samples
     **********************************************************************************/

  //bandOutputFactor = bandCouplingFactor[currentBand] * CWPowerCalibrationFactor[currentBand] / CWPowerCalibrationFactor[1];
  //bandOutputFactor = bandCouplingFactor[currentBand] * 1.0;
  bandOutputFactor = 0.1;

  // generate I and Q for the transmit or receive calibration
  if(calibrateItem == 2 || calibrateItem == 3) {
    arm_scale_f32(cosBuffer3, bandOutputFactor, audioBufferL_EX, 256);  // use pre-calculated sin & cos instead of Hilbert
    arm_scale_f32(sinBuffer3, bandOutputFactor, audioBufferR_EX, 256);  // sidetone = 3000
  }

  UpdateIQCorrection();

  // adjust IQ signals for amplitude and phase correction factors
  if(bands[currentBand].demod == DEMOD_LSB) {
    arm_scale_f32(audioBufferL_EX, -IQXAmpCorrectionFactor[currentBand], audioBufferL_EX, 256);
    IQPhaseCorrection(audioBufferL_EX, audioBufferR_EX, IQXPhaseCorrectionFactor[currentBand], 256);
  } else {
    if(bands[currentBand].demod == DEMOD_USB || bands[currentBand].demod == DEMOD_FT8) {
      arm_scale_f32(audioBufferL_EX, IQXAmpCorrectionFactor[currentBand], audioBufferL_EX, 256);
      IQPhaseCorrection(audioBufferL_EX, audioBufferR_EX, IQXPhaseCorrectionFactor[currentBand], 256);
    }
  }

  /*
  //24KHz effective sample rate here
  arm_fir_interpolate_f32(&FIR_int1_EX_I, audioBufferL_EX, audioBufferTemp, 256);

  // interpolation-by-4,  48KHz effective sample rate here
  arm_fir_interpolate_f32(&FIR_int2_EX_I, audioBufferTemp, audioBufferL_EX, 512);

  // and again for R channel
  arm_fir_interpolate_f32(&FIR_int1_EX_Q, audioBufferR_EX, audioBufferTemp, 256);
  arm_fir_interpolate_f32(&FIR_int2_EX_Q, audioBufferTemp, audioBufferR_EX, 512);
  */

  // scale to give equivalent peak as SSB USB cal
  //arm_scale_f32(audioBufferL_EX, 0.126, audioBufferL_EX, 256);
  //arm_scale_f32(audioBufferR_EX, 0.126, audioBufferR_EX, 256);

  // scale to give +10dB peak (actually gave +21dB)
  //arm_scale_f32(audioBufferL_EX, 3.16, audioBufferL_EX, 256);
  //arm_scale_f32(audioBufferR_EX, 3.16, audioBufferR_EX, 256);

  //arm_scale_f32(audioBufferL_EX, 2.0, audioBufferL_EX, 256);
  //arm_scale_f32(audioBufferR_EX, 2.0, audioBufferR_EX, 256);

  // are there at least 16 blocks available in each channel
  if((uint32_t)Q_in_L.available() > 2 && (uint32_t)Q_in_R.available() > 2) {

    q15_t q15_buffer_LTemp[256];
    q15_t q15_buffer_RTemp[256];
    Q_out_L_Ex.setBehaviour(AudioPlayQueue::NON_STALLING);
    Q_out_R_Ex.setBehaviour(AudioPlayQueue::NON_STALLING);
    arm_float_to_q15(audioBufferL_EX, q15_buffer_LTemp, 256);
    arm_float_to_q15(audioBufferR_EX, q15_buffer_RTemp, 256);
    Q_out_L_Ex.play(q15_buffer_LTemp, 256);
    Q_out_R_Ex.play(q15_buffer_RTemp, 256);
    //Q_out_L_Ex.setBehaviour(AudioPlayQueue::ORIGINAL);
    //Q_out_R_Ex.setBehaviour(AudioPlayQueue::ORIGINAL);

    // get audio samples from the audio  buffers and convert them to float
    // read in 32 blocks á 128 samples in I and Q
    for(unsigned i = 0; i < 2; i++) {
      /**********************************************************************************
          Using arm_Math library, convert to float one buffer_size.
          Float_buffer samples are now standardized from > -1.0 to < 1.0
      **********************************************************************************/
      arm_q15_to_float(Q_in_R.readBuffer(), &audioBufferL[128 * i], 128);
      arm_q15_to_float(Q_in_L.readBuffer(), &audioBufferR[128 * i], 128);
      Q_in_L.freeBuffer();
      Q_in_R.freeBuffer();
    }

    rfGainValue = pow(10, (float)rfGainAllBands / 20);
    arm_scale_f32(audioBufferL, rfGainValue, audioBufferL, 256);
    arm_scale_f32(audioBufferR, rfGainValue, audioBufferR, 256);

    /**********************************************************************************
      Scale the data buffers by the RFgain value defined in bands[currentBand] structure
    **********************************************************************************/
    arm_scale_f32(audioBufferL, recBandFactor[currentBand], audioBufferL, 256);
    arm_scale_f32(audioBufferR, recBandFactor[currentBand], audioBufferR, 256);

    // Manual IQ amplitude correction
    if(bands[currentBand].demod == DEMOD_LSB) {
      arm_scale_f32(audioBufferL, -IQAmpCorrectionFactor[currentBand], audioBufferL, 256);
      IQPhaseCorrection(audioBufferL, audioBufferR, IQPhaseCorrectionFactor[currentBand], 256);
    } else {
      if(bands[currentBand].demod == DEMOD_USB || bands[currentBand].demod == DEMOD_FT8) {
        arm_scale_f32(audioBufferL, -IQAmpCorrectionFactor[currentBand], audioBufferL, 256);
        IQPhaseCorrection(audioBufferL, audioBufferR, IQPhaseCorrectionFactor[currentBand], 256);
      }
    }

    FreqShift1(256);

    if(spectrumZoom == 0) {  // && display_S_meter_or_spectrum_state == 1)
      Calc1xFreqSpec();
    }

    // Kick off frequency spectrum FFT routine only once for each audio process loop
    if(spectrumZoom != 0) {
      if(updateSpectrumData && (reqPasses == 20)) {
        passes = 0;

        // calc passes needed to buffer a complete frequency spectrum at the current zoom factor
        // and sample rate.  At 192kkHz sample rate, the zoom factor alone determines the passes
        // required as the sample rate term below is 0.  At 44.1kHz sample rate, zoom is limited
        // to 2x and 4x (22kHz/11kHz BW which is roughly equivalent to an 8x or 16x zoom).
        // so the passes required based on zoom factor will always be 1 but the passes required
        // based on sample rate are 4 or 8.
        //          <----------------- zoom factor ------------------>   <----- sample rate ----->
        reqPasses = (spectrumZoom < 3 ? 1 : ((1 << spectrumZoom) / 4)) + 2048 / (256) - 1;
      }
      if(passes < reqPasses) {
        passes++;
        if(passes == reqPasses) {
          // flag that we're ready to update frequency spectrum
          // no need to reset passes, we won't pass through this
          // block again until the next time updateSpectrumData is set
          updateFreqSpec = true;
          reqPasses = 20;
          passes = 20;
        }
        CalcZoomFreqSpec(256, updateFreqSpec);
      }
    }
  }
  return updateFreqSpec;
}

/*****
  Purpose: Show Spectrum display modified for IQ calibration.
           This is similar to the function used for normal reception, however, it has
           been simplified and streamlined for calibration.
*****/
int hLo = 0, hHi = 0;
FLASHMEM void FT8ShowSpectrum2() {
  int x1 = 0;
  float adjdB = 0.0;
  int capture_bins = 10;  // ets the number of bins to scan for signal peak
  static int currentNF = 0;

  pixelnew[0] = 0;
  pixelnew[1] = 0;

  if(liveNoiseFloorFlag != 1) {
    currentNF = currentNoiseFloor[currentBand];
  }

  //  This is the "spectra scanning" for loop.  During calibration, only small areas of the spectrum need to be examined.
  //  If the entire 512 wide spectrum is used, the calibration loop will be slow and unresponsive.
  //  The scanning areas are determined by receive versus transmit calibration, and LSB or USB.  Thus there are 4 different scanning zones.
  //  All calibrations use a 0 dB reference signal and an "undesired sideband" signal which is to be minimized relative to the reference.
  //  Thus there is a target "bin" for the reference signal and another "bin" for the undesired sideband.
  //  The target bin locations are used by the for-loop to sweep a small range in the FFT.  A maximum finding function finds the peak signal strength.
  int cal_bins[2] = {0, 0};
  if(calTypeFlag == 0 && bands[currentBand].demod == DEMOD_LSB) {
    cal_bins[0] = 310;
    cal_bins[1] = 460;
  }  // Receive calibration, LSB
  if(calTypeFlag == 0 && (bands[currentBand].demod == DEMOD_USB || bands[currentBand].demod == DEMOD_FT8)) {
    cal_bins[0] = 65;
    cal_bins[1] = 192;
  }  // Receive calibration, USB
  if(calTypeFlag == 1 && bands[currentBand].demod == DEMOD_LSB) {
    cal_bins[0] = 240;
    cal_bins[1] = 305;
  }  // Transmit calibration, LSB
  if(calTypeFlag == 1 && (bands[currentBand].demod == DEMOD_USB || bands[currentBand].demod == DEMOD_FT8)) {
    cal_bins[0] = 209;
    cal_bins[1] = 273;
  }  // Transmit calibration, USB

  //  There are 2 for-loops, one for the reference signal and another for the undesired sideband.
  for(x1 = 0; x1 < SPECTRUM_RES -1; x1++) adjdB = FT8PlotCalSpectrum(x1, cal_bins, capture_bins, currentNF);
  //for(x1 = cal_bins[0] - capture_bins; x1 < cal_bins[0] + capture_bins; x1++) adjdB = FT8PlotCalSpectrum(x1, cal_bins, capture_bins, currentNF);
  //for(x1 = cal_bins[1] - capture_bins; x1 < cal_bins[1] + capture_bins; x1++) adjdB = FT8PlotCalSpectrum(x1, cal_bins, capture_bins, currentNF);

  // Plot carrier during transmit cal, do not return a dB value:
  if(calTypeFlag == 1) {
    for(x1 = cal_bins[0] + 20; x1 < cal_bins[1] - 20; x1++) FT8PlotCalSpectrum(x1, cal_bins, capture_bins, currentNF);
  }


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
    //Serial.println("at 3");
  }

  // print sideband supression
  tft.setFontScale((enum RA8875tsize)0);
  tft.fillRect(350, 125, 50, tft.getFontHeight(), RA8875_BLACK);
  tft.setCursor(350, 125);  // 350, 125
  tft.print(adjdB, 1);

  //  at least a partial waterfall is necessary.  It seems to provide some important timing function
  //tft.BTE_move(WATERFALL_L, WATERFALL_T, WATERFALL_W, WATERFALL_H, WATERFALL_L, WATERFALL_T + 1, 1, 2);
  //while(tft.readStatus())
  //  ;
}

/*****
  Purpose:  Plot Calibration Spectrum   //  KF5N 7/2/2023
            This function plots a partial spectrum during calibration only.
            This is intended to increase the efficiency and therefore the responsiveness of the calibration encoder.
            This function is called by FT8ShowSpectrum2() in two for-loops.  One for-loop is for the refenence signal,
            and the other for-loop is for the undesired sideband.
  Parameter list:
    int x1, where x1 is the FFT bin.
    cal_bins[2] locations of the desired and undesired signals
    capture_bins width of the bins used to display the signals
  Return value:
    float returns the adjusted value in dB
*****/
FLASHMEM float FT8PlotCalSpectrum(int x1, int cal_bins[2], int capture_bins, int currentNF) {
  float adjdB = 0.0;
  int16_t adjAmplitude = 0;
  int16_t refAmplitude = 0;
  uint32_t index_of_max;     // This variable is not currently used, but it is required by the ARM max function.  KF5N
  int16_t yPlot, y1Plot;
  static int yOldPlot[SPECTRUM_RES];
  bool drawSpec = true, eraseSpec = true, inBoxLow = true, inBoxHigh = true;
  bool updateSpectrumData = x1 == 0 ? true : false;

  if(x1 == (cal_bins[0] - capture_bins)) {  // Set flag at revised beginning.  KF5N
    updateSpectrumData = true;                   //Set flag so the display data are saved only once during each display refresh cycle at the start of the cycle, not 512 times
    ShowBandwidthBarValues();                         // Without this call, the calibration value in dB will not be updated.  KF5N
  } else updateSpectrumData = false;              //  Do not save the the display data for the remainder of the
  //}

  if(updateSpectrumData) {
    while(!FT8ProcessIQData2(updateSpectrumData)) ;  // Call the Audio process from within the display routine to eliminate conflicts with drawing the spectrum and waterfall displays

  } else {
    FT8ProcessIQData2(updateSpectrumData);  // Call the Audio process from within the display routine to eliminate conflicts with drawing the spectrum and waterfall displays
  }

  // calculate the freq spectrum plot value; pixelnew spectrum is calculated in CalcZoomFreqSpec
  yPlot = spectrumNoiseFloor - pixelnew[x1] - currentNF + 50;
  y1Plot = spectrumNoiseFloor - pixelnew[x1 + 1] - currentNF + 50;

  // create rough spectrum histogram if auto noise floor is active
  // the frequency spectrum is 150 pixels high, let's create
  // rough histogram 30 bins wide (or 5 pixels each, ie, divide by 5)
  // you might think divide by 4 would be more efficient as 2 right shifts
  // but right shift of a negative number is implimentation specific
  // and I want to keep the negative numbers here
  if(liveNoiseFloorFlag == 1) {
    int specPlotY = spectrumNoiseFloor - yPlot; // actual spectrum value at current noise floor
    int bin = specPlotY / 5;                    // divide by 5 to get histogram bin

    // hLo and hHi capture spectrum at or outside the spectrum display extremes
    // this is all we need to automatically set the noise floor
    // *** TODO: consider using other histogram bins to more rapidly set noise flow ***
    if(bin < 1) {
      hLo += 1;
    } else if(bin >= 29) {
      hHi += 1;
    }
  }

  // clear erase flag if we don't need to erase anything
  if((yOldPlot[x1] == SPECTRUM_BOTTOM) && (yOldPlot[x1 + 1] == SPECTRUM_BOTTOM)) {
    eraseSpec = false;
  }
  if((yOldPlot[x1] == SPECTRUM_TOP_Y) && (yOldPlot[x1 + 1] == SPECTRUM_TOP_Y)) {
    eraseSpec = false;
  }

  // erase the old spectrum if needed
  if(eraseSpec && (displayState == DISPLAY_T41)) {
    tft.drawLine(SPECTRUM_LEFT_X + x1, yOldPlot[x1 + 1], SPECTRUM_LEFT_X + x1, yOldPlot[x1], RA8875_BLACK);
  }

  // Find the maximums of the desired and undesired signals.
  if(bands[currentBand].demod == DEMOD_LSB) {
    arm_max_q15(&pixelnew[(cal_bins[0] - capture_bins)], capture_bins * 2, &refAmplitude, &index_of_max);
    arm_max_q15(&pixelnew[(cal_bins[1] - capture_bins)], capture_bins * 2, &adjAmplitude, &index_of_max);
  }
  if(bands[currentBand].demod == DEMOD_USB || bands[currentBand].demod == DEMOD_FT8) {
    arm_max_q15(&pixelnew[(cal_bins[0] - capture_bins)], capture_bins * 2, &adjAmplitude, &index_of_max);
    arm_max_q15(&pixelnew[(cal_bins[1] - capture_bins)], capture_bins * 2, &refAmplitude, &index_of_max);
  }

  // prevent drawing spectrum outside of the spectrum area
  // also clear draw flag if we don't need to draw anything
  if(yPlot > SPECTRUM_BOTTOM) {
    //Serial.println(yPlot);
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
  //if(drawSpec && (displayState == DISPLAY_T41)) {
  //  tft.drawLine(SPECTRUM_LEFT_X + x1, y1Plot, SPECTRUM_LEFT_X + x1, yPlot, RA8875_YELLOW);
  //  Serial.println("at 1");
  //} else {
  //  Serial.println("at 2");
  //}
    tft.drawLine(SPECTRUM_LEFT_X + x1, y1Plot, SPECTRUM_LEFT_X + x1, yPlot, RA8875_YELLOW);

  // save plot value to erase spectrum next loop
  yOldPlot[x1] = yPlot;

  if(calTypeFlag == 0) {  // Receive Cal
    adjdB = ((float)adjAmplitude - (float)refAmplitude) / 1.95;
    tft.writeTo(L2);
    if(bands[currentBand].demod == DEMOD_LSB) {
      tft.fillRect(450, SPECTRUM_TOP_Y + 20, 20, 135 - 6, DARK_RED);     // SPECTRUM_TOP_Y = 100
      tft.fillRect(300, SPECTRUM_TOP_Y + 20, 20, 135 - 6, RA8875_BLUE);  // h = SPECTRUM_HEIGHT + 3
    } else {                                                           // SPECTRUM_HEIGHT = 150 so h = 153
      tft.fillRect(55, SPECTRUM_TOP_Y + 20, 20, 135 - 6, DARK_RED);
      tft.fillRect(182, SPECTRUM_TOP_Y + 20, 20, 135 - 6, RA8875_BLUE);
    }
  } else {                                                       //Transmit Cal
    adjdB = ((float)adjAmplitude - (float)refAmplitude) / 1.95;  // Cast to float and calculate the dB level.  KF5N
    tft.writeTo(L2);
    if(bands[currentBand].demod == DEMOD_LSB) {
      tft.fillRect(295, SPECTRUM_TOP_Y + 20, 20, 135 - 6, DARK_RED);  // Adjusted height due to other graphics changes.  KF5N August 3, 2023
      tft.fillRect(230, SPECTRUM_TOP_Y + 20, 20, 135 - 6, RA8875_BLUE);
    } else {
      if(bands[currentBand].demod == DEMOD_USB || bands[currentBand].demod == DEMOD_FT8) {  //mode == DEMOD_LSB
        tft.fillRect(199, SPECTRUM_TOP_Y + 20, 20, 135 - 6, DARK_RED);
        tft.fillRect(263, SPECTRUM_TOP_Y + 20, 20, 135 - 6, RA8875_BLUE);
      }
    }
  }
  tft.writeTo(L1);
  return adjdB;
}
