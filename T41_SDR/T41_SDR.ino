/*********************************************************************************************

  This comment block must appear in the load page (e.g., main() or setup()) in any source code
  that uses code presented as whole or part of the T41-EP source code.

  (c) Frank Dziock, DD4WH, 2020_05_8
  "TEENSY CONVOLUTION SDR" substantially modified by Jack Purdum, W8TEE, and Al Peter, AC8GY

  This software is made available under the GNU GPLv3 license agreement. If commercial use of this
  software is planned, we would appreciate it if the interested parties contact Jack Purdum, W8TEE,
  and Al Peter, AC8GY.

*********************************************************************************************/

// setup() and loop() at the bottom of this file

#include <TimeLib.h> // Part of Teensy Time library

#include "SDT.h"

#include "AudioConfig.h"
#include "Bearing.h"
#include "Button.h"
#include "CWProcessing.h"
#include "CW_Excite.h"
#include "Display.h"
#include "DSP_Fn.h"
#include "EEPROM.h"
#include "Encoders.h"
#include "Exciter.h"
#include "Filter.h"
#include "FIR.h"
#include "InfoBox.h"
#include "Menu.h"
#include "Noise.h"
#include "Process.h"
#include "Tune.h"
#include "Utility.h"

// special features
#include "Beacon.h"
#include "debug.h"
#include "keyboard.h"
#include "keyer.h"
#include "mouse.h"
#include "remote.h"
#include "t41Beacon.h"
#include "t41Control.h"
#include "t41USBHost.h"
#include "wsjt.h"

// *** need to pull what we want from these ***
//#include "fir_cmsis_5k.h"
//#include "fir_alt.h"

//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

float sampleRate, intermediateFreq;

int radioState, lastState;

int volSetting = 0;

float32_t DMAMEM audioBufferL[2048];
float32_t DMAMEM audioBufferR[2048];
float32_t DMAMEM audioBufferL_EX[2048];
float32_t DMAMEM audioBufferR_EX[2048];
float32_t DMAMEM audioBufferTemp[2048];

/*
typedef struct {
  long freq;      // Current frequency in Hz
  long fBandLow;  // Lower band edge
  long fBandHigh; // Upper band edge
  const char* name; // name of band
  int demod;
  int FHiCut;
  int FLoCut;
  int RFgain;
  long calFreq; // receive IQ calibration frequency, set to 0 to skip calibration of a specific band
  float32_t gainCorrection; // is hardware dependent and has to be calibrated ONCE and hardcoded in the band table
  int AGC_thresh;
  int16_t pixel_offset;
} band;
*/

// gainCorrection used in signal strength calculation
// set with signal from AD3 (1mW -73dB external attenuation, 223.6mVrms @ 1kHz w/ default freq for band; see "Wavegen for RF in - S9 - 1mW with 73dB external atten.dwf3work")
band bands[NUMBER_OF_BANDS] = {
//  freq      band low   band hi   name    demod        Hi   Low     Gain  calFreq      gain                    AGC   pixel
//                                                       filter                         correct                       offset
//  freq      fBandLow   fBandHigh name    demod       FHiCut FLoCut RFgain             gainCorrection
    3700000,  3500000,   4000000,  "80M",  DEMOD_LSB,  3000, 200,    1,    3750000,     GAIN_CORRECTION_80M,    20,    20,
    7150000,  7000000,   7300000,  "40M",  DEMOD_LSB,  3000, 200,    1,    7150000,     GAIN_CORRECTION_40M,    20,    20,
    14200000, 14000000, 14350000,  "20M",  DEMOD_USB,  3000, 200,    1,    14175000,    GAIN_CORRECTION_20M,    20,    20,
    18100000, 18068000, 18168000,  "17M",  DEMOD_USB,  3000, 200,    1,    18118000,    GAIN_CORRECTION_17M,    20,    20,
    21200000, 21000000, 21450000,  "15M",  DEMOD_USB,  3000, 200,    1,    21225000,    GAIN_CORRECTION_15M,    20,    20,
    24920000, 24890000, 24990000,  "12M",  DEMOD_USB,  3000, 200,    1,    24940000,    GAIN_CORRECTION_12M,    20,    20,
//    28350000, 28000000, 29700000,  "10M",  DEMOD_USB,  3000, 200,    1,    28850000,    GAIN_CORRECTION_10M,    20,    20 // gainCorrection set to 12m band value as AD3 can't generate this signal
    28350000, 28000000, 29700000,  "10M",  DEMOD_USB,  3000, 200,    1,           0,    GAIN_CORRECTION_10M,    20,    20 // gainCorrection set to 12m band value as AD3 can't generate this signal
};

int bandswitchPins[] = {
  FILTERPIN80M,  // 80M
  FILTERPIN40M,  // 40M
  FILTERPIN20M,  // 20M
  FILTERPIN15M,  // 17M
  FILTERPIN15M,  // 15M
  0,   // 12M  Note that 12M and 10M both use the 10M filter, which is always in (no relay).  KF5N September 27, 2023.
  0    // 10M
};

// local variables
long long oldCenterFreq = centerFreq; // simplifies v12 transmit recovery

//-------------------------------------------------------------------------------------------------------------
// Forwards
//-------------------------------------------------------------------------------------------------------------

int SetI2SFreq(int freq);

//-------------------------------------------------------------------------------------------------------------
// Code
//-------------------------------------------------------------------------------------------------------------

FLASHMEM void InitializeDataArrays() {
  InitFFTArrays();

  SET_VAR(pixelnew, SPECTRUM_BOTTOM);
  CLEAR_VAR(NR_FFT_buffer);
  CLEAR_VAR(NR_output_audio_buffer);
  CLEAR_VAR(NR_last_iFFT_result);
  CLEAR_VAR(NR_last_sample_buffer_L);
  CLEAR_VAR(NR_last_sample_buffer_R);
  CLEAR_VAR(NR_M);
  CLEAR_VAR(NR_lambda);
  CLEAR_VAR(NR_G);
  CLEAR_VAR(NR_SNR_prio);
  CLEAR_VAR(NR_SNR_post);
  CLEAR_VAR(NR_Hk_old);
  CLEAR_VAR(NR_X);
  CLEAR_VAR(NR_Nest);
  CLEAR_VAR(NR_Gts);
  CLEAR_VAR(NR_E);
  CLEAR_VAR(ANR_d);
  CLEAR_VAR(ANR_w);
  CLEAR_VAR(LMS_StateF32);
  CLEAR_VAR(LMS_NormCoeff_f32);
  CLEAR_VAR(LMS_nr_delay);

  // initialize various filters
  InitFIRFilters();
  InitZoomFFTFilter();
  InitSpectralNoiseReduction();
  InitLMSNoiseReduction();

  // this needs to come after above
  InitAMDemodBiquadFilter();

  // prepare 750Hz signal buffer
  GenSineToneBuffers(8);
}

FLASHMEM void Splash() {
  int centerTxt;
  int line1_Y = YPIXELS / 10;
  int line2_Y = line1_Y + 100;
  int line3_Y = line2_Y + 50;
  int line4_Y = line3_Y + 150;
  int line5_Y = line4_Y + 40;
  //int line6_Y = YPIXELS / 2 + 110;
  //int line7_Y = line6_Y + 50;

  // 50 char max for 800x480 display with font scale = 1:
  //                     "          1         2         3         4"
  //                     "01234567890123456789012345678901234567890123456789";
  const char*line1Txt = "T41-EP";
  const char*line2Txt = "Version: "; // + VERSION
  const char*line3Txt = "By: Terrance Robertson, KN6ZDE";
  const char*line4Txt = "Based on design by: Al Peter, AC8GY and Jack Purdum, W8TEE";
  const char*line5Txt = "";
  //const char*line6Txt = "Property of:"; // line 7 MY_CALL

  tft.fillWindow(RA8875_BLACK);

  tft.setFontScale(3);
  tft.setTextColor(RA8875_GREEN);
  centerTxt = (XPIXELS - strlen(line1Txt) * tft.getFontWidth()) / 2;
  tft.setCursor(centerTxt, line1_Y);
  tft.print(line1Txt);

  tft.setFontScale(1);
  tft.setTextColor(RA8875_YELLOW);
  centerTxt = (XPIXELS - (strlen(line2Txt) + strlen(VERSION)) * tft.getFontWidth()) / 2;
  tft.setCursor(centerTxt, line2_Y);
  tft.print("Version: ");
  tft.print(VERSION);

  centerTxt = (XPIXELS - strlen(line3Txt) * tft.getFontWidth()) / 2;
  tft.setCursor(centerTxt, line3_Y);
  tft.print(line3Txt);

  tft.setFontScale(0);
  tft.setTextColor(RA8875_WHITE);
  centerTxt = (XPIXELS - strlen(line4Txt) * tft.getFontWidth()) / 2;
  tft.setCursor(centerTxt, line4_Y);
  tft.print(line4Txt);
  centerTxt = (XPIXELS - strlen(line5Txt) * tft.getFontWidth()) / 2;
  tft.setCursor(centerTxt, line5_Y);
  tft.print(line5Txt);

/*
  tft.setFontScale(1);
  centerTxt = (XPIXELS - strlen(line6Txt) * tft.getFontWidth()) / 2;
  tft.setCursor(centerTxt, line6_Y);
  tft.print(line6Txt);

  tft.setTextColor(RA8875_GREEN);
  centerTxt = (XPIXELS - strlen(MY_CALL) * tft.getFontWidth()) / 2;
  tft.setCursor(centerTxt, line7_Y);
  tft.print(MY_CALL);
*/
  delay(1000);
  //delay(SPLASH_DELAY);
  tft.fillWindow(RA8875_BLACK);
}

/*****
  Purpose: perform a soft reset of the radio
              This resets the user modifiable radio settings to the startup state
*****/
FLASHMEM void SoftReset() {
  // can't use any working variables until after this, we can get rid of this when we use EEPROMData
  // skip for now to facilitate testing/dev, don't need to reset when shifting between v66-9 and this version
  //LoadOpVars();

  // reset sample rate and IF
  sampleRate = 192000.0;
  intermediateFreq = 48000.0;

  splitVFO = false;
  SoftResetHardware();

  SetKeyPowerUp();  // Use keyType and paddleFlip to configure key GPIs
  SetDitLength(currentWPM);
  SetTransmitDitLength();
  menuEncoderMove = 0;
  fineTuneEncoderMove = 0L;

  mainMenuIndex = 0;             // Changed from middle to first. Do Menu Down to get to Calibrate quickly
  secondaryMenuIndex = -1;       // -1 means haven't determined secondary menu
  menuStatus = NO_MENUS_ACTIVE;  // Blank menu field

  // set T41 last state different from radio state indicating a state change
  // so receiver will be configured on the first pass through loop()
  lastState = -1;

  // the following items in addition to the radio state change
  // are sufficient to fully draw the display
  DrawStaticDisplayItems();
  ShowOperatingStats();
  ShowSpectrumdBScale();
  ShowBandwidthBarValues();
  DrawBandwidthBar();
  UpdateInfoBox();
  DrawAudioFilterLines();

  AGCPrep(); // no audio without this unless AGC is off

  NCOFreq = 0;
  ResetTuning();
  SetBandRelay(HIGH);
}

// *** for testing ***
//extern "C" uint8_t external_psram_size;

FLASHMEM void setup() {
  Serial.begin(9600);

  delay(1000);

  // Check for PSRAM chip(s) installed
  //uint8_t size = external_psram_size;
  //if (size == 0) {
  //  Serial.println("No PSRAM Installed");
  //  //tft.println("No PSRAM Installed");
  //} else {
  //  Serial.printf("PSRAM Memory Size = %d Mbyte\n", size);
  //  //tft.printf("PSRAM Memory Size = %d Mbyte\n", size);
  //  //tft.println();
  //}

  // set system time
  // see: https://github.com/PaulStoffregen/Time
  setSyncProvider(GetTeensyTime); // get the time from the RTC
  setTime(now()); // set system time
  SetTeensyTime(now()); // reset the RTC to current time

  // set up Teensy pins that aren't handled elsewhere
  pinMode(RXTX, OUTPUT);
  pinMode(PTT, INPUT_PULLUP);

  pinMode(KEYER_DIT_INPUT_TIP, INPUT_PULLUP);
  pinMode(KEYER_DAH_INPUT_RING, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(KEYER_DIT_INPUT_TIP), KeyTipOn, CHANGE);
  attachInterrupt(digitalPinToInterrupt(KEYER_DAH_INPUT_RING), KeyRingOn, CHANGE);

  InitDisplay();
  Splash();

  sdCardPresent = InitializeSDCard();  // Is there an SD card that can be initialized?
  EEPROMStartup();
  sdCardPresent = SDPresentCheck();
#ifdef DEBUG
  EEPROMShow();
#endif

  delay(100L);

  sampleRate = 192000.0;
  intermediateFreq = 48000.0;

  InitSI5351();
  AudioSetup();

  InitializeDataArrays();

  InitHardware();
  SoftReset();

#ifdef USB_HOST_SUPPORT
  UsbHostSetup();
#endif

#ifdef HOST_KEYBOARD_MOUSE_SUPPORT
  // draw a white rectangle to layer 1 to mask the cursor copy area
  //tft.fillRect(XPIXELS - 20, TIME_Y, 16, 32, RA8875_WHITE);
  tft.fillRect(0, 0, 16, 32, RA8875_WHITE);
  SetMouseArea(0, 0, XPIXELS, YPIXELS);
  HighlightIBItem(IB_ITEM_FINE, RA8875_GREEN);
#endif

  //memCheck = true;
  PrimeMallInfo();

#ifdef NO_DISPLAY
  T41ControlSetup();
#endif
  //T41BeaconSetup();
  //WSJTControlSetup();
  //T41ControlSetup();

  KeyerSetup(); // testing only

  // initialize Teensy temperature monitor
  // temp_check_frequency = 0x03U;  //updates the temp value at a RTC/3 clock rate
  // 0xFFFF determines a 2 second sample rate period
  //initTempMon(temp_check_frequency, lowAlarmTemp, highAlarmTemp, panicAlarmTemp);
  initTempMon(0x03U, 25U, 85U, 90U);  // 85U = 42 degrees C?
  // this starts the measurements
  TEMPMON_TEMPSENSE0 |= 0x2U;

#ifdef PROFILER_ACTIVE
  pinMode(2, OUTPUT);
  digitalWrite(2, LOW);
  pinMode(30, OUTPUT);
  digitalWrite(30, LOW);
  pinMode(4, OUTPUT);
  digitalWrite(4, LOW);
  //pinMode(5, OUTPUT);
  //digitalWrite(5, LOW);
#endif
}

#ifdef DEBUG
extern unsigned long _heap_start;
extern unsigned long _heap_end;
extern char *__brkval;
int freeram() {
  return (char *)&_heap_end - __brkval;
}
#endif

void ConfigRadioState() {
  ConfigRadioStateHardware();

  switch(radioState) {
    case SSB_RECEIVE_STATE:
      break;

    case SSB_TRANSMIT_STATE:
#ifdef USE_MIC_COMPRESSION
      if(compressorFlag == 1) {
        SetupMicCompressors((float)currentMicThreshold, .1, 2.0);
      } else if(compressorFlag == 0) {
        SetupMicCompressors(0.0, 0.01, 0.01);
      }
#endif
      sgtl5000_1.micGain(10);
      break;

    case CW_RECEIVE_STATE:
      if((decoderFlag == ON) && (lastState != CW_RECEIVE_STATE)) {
        InitCW();
      }
      break;

    case CW_TRANSMIT_STRAIGHT_STATE:
    case CW_TRANSMIT_KEYER_STATE:
      break;

    case DATA_RECEIVE_STATE:
      // *** TODO: consider moving initialization stuff from ChangeDemodMode and ChangeMode here ***
      break;

    default:
      break;
  }
}

FASTRUN void loop() {
  int pushButtonSwitchIndex = -1;
  int valPin;
  unsigned long cwTransmitTimer;

#ifdef PROFILER_ACTIVE
  static int loopStart = 0;
  loopStart = !loopStart;
  digitalWrite(2, loopStart);
#endif

  HardwareLoopStart();

#ifdef T41_USB_AUDIO
  // *** There is only one USB serial object available with USB audio enabled.  The Serial object
  // is reserved for WSJT-X use.  Any other use could disrupt WSJT-X control of the T41.  The
  // wsjt module provides for to communication with the WSJT-X app and allows setting the T41 clock
  // with the SetT41Clock PC app. ***
  WSJTLoop();
#endif

#ifdef AUDIO_STATS
  StartAudioStats();
#endif

  if(memCheck) {
    if(++loopCounter == 100) {
      memInfo();
      loopCounter = 0;
    }
  }

  if(beaconFlag) {
    BeaconLoop();
  }

#ifdef DEBUG_LOOP
  EnterLoop();
#endif

  // check for UI button press and process accordingly
  valPin = ReadSelectedPushButton();
  if(valPin != BOGUS_PIN_READ) {
    pushButtonSwitchIndex = ProcessButtonPress(valPin);
    ExecuteButtonPress(pushButtonSwitchIndex);
  }

#ifdef DEBUG_LOOP
  ButtonInfoOut(valPin, pushButtonSwitchIndex);
#endif

  //  State detection
  if(radioMode == SSB_MODE && digitalRead(PTT) == HIGH) {
    radioState = SSB_RECEIVE_STATE;
  }
  if(radioMode == SSB_MODE && digitalRead(PTT) == LOW) {
    radioState = SSB_TRANSMIT_STATE;
  }
  if(radioMode == CW_MODE && (digitalRead(paddleDit) == HIGH && digitalRead(paddleDah) == HIGH)) {
    radioState = CW_RECEIVE_STATE;
  }
  if(radioMode == CW_MODE && (digitalRead(paddleDit) == LOW && keyType == 0)) {
    radioState = CW_TRANSMIT_STRAIGHT_STATE;
  }
  if(radioMode == CW_MODE && (keyPressedOn == 1 && keyType == 1)) {
    radioState = CW_TRANSMIT_KEYER_STATE;
    keyPressedOn = 0;
  }

  if(radioMode == DATA_MODE) {
    //Serial.print("ft8PTT: "); Serial.println(ft8PTT);
    if(ft8PTT) {
      radioState = DATA_TRANSMIT_STATE;
    } else {
      radioState = DATA_RECEIVE_STATE;
    }

    // *** TODO: consider best place to do this ***
    // *** TODO: this needs work ***
    switch(bands[currentBand].demod) {
      case DEMOD_FT8:
        if(sampleRate > 50000) {
          sampleRate = 44100.0;
          intermediateFreq = 11025.0;
          // using 48k sample rate doesn't change FT8 transmision
          //sampleRate = 48000.0;
          //intermediateFreq = 12000.0;
          SetI2SFreq(sampleRate);
          InitFFTArrays();
          SetZoom(1);
          //InitZoomFFTFilter(); // *** TODO: can save some memory by specifying block size if will operate in FT8 a lot ***
          InitHilbertFilters();
          SetupDemodFilterBW();
          //ShowSpectrumFreqValues();
          DrawAudioSpectContainer();
          DrawAudioFilterLines();
          ResetTuning();
        }
        break;

      default:
        if(sampleRate < 50000) {
          sampleRate = 192000.0;
          intermediateFreq = 48000.0;
          SetI2SFreq(sampleRate);
          InitFFTArrays();
          SetZoom(1);
          InitHilbertFilters();
          SetupDemodFilterBW();
          DrawAudioSpectContainer();
          DrawAudioFilterLines();
          //ShowSpectrumFreqValues();
          //ShowOperatingStats();
        }
        break;
    }
  } else {
    if(sampleRate < 50000) {
      sampleRate = 192000.0;
      intermediateFreq = 48000.0;
      SetI2SFreq(sampleRate);
      InitFFTArrays();
      SetZoom(1);
      InitHilbertFilters();
      SetupDemodFilterBW();
      DrawAudioSpectContainer();
      DrawAudioFilterLines();
      //ShowSpectrumFreqValues();
      //ShowOperatingStats();
    }
  }

  if(radioState != lastState) {
    // cleanup last state
    switch(lastState) {
      case CW_RECEIVE_STATE:
        if((radioMode != CW_MODE) && (decoderFlag == ON)) {
          // free up CW decoder memory
          ExitCW();
        }
        break;

      case DATA_RECEIVE_STATE:
        // *** TODO: consider moving exit stuff from ChangeDemodMode and ChangeMode here ***
        break;

      case DATA_TRANSMIT_STATE:
        digitalWrite(RXTX, LOW); // turn off TX relay
        break;

      case CALIBRATE_TRANSMIT_STATE:
        break;

      default:
        break;
    }

    ConfigAudioState(radioState);
    ConfigRadioState();
    SetFreq();  // Update frequencies if the radio state has changed
    ShowTransmitReceiveStatus();
  }

  // *** TODO: consider if a control update is proper here ***
  ProcessControls();

  // process radio state
  switch(radioState) {
    case SSB_RECEIVE_STATE:
    case CW_RECEIVE_STATE:
    case DATA_RECEIVE_STATE:
      switch(displayState) {
        case DISPLAY_T41:
          ShowSpectrum();
          break;

        case DISPLAY_BEACON_MONITOR:
        default:
        // process control and IQ signals without updating display
        // (other events may still update display, clock for example)
        // *** TODO: many control tasks still update screen.  Fix this. ***
        YieldToProcess();
        break;
      }
      //if(radioState == DATA_RECEIVE_STATE) Serial.println("at 4");
      break;

    case SSB_TRANSMIT_STATE:
      digitalWrite(RXTX, HIGH); // turn on TX relay

      while(digitalRead(PTT) == LOW) {
        PrepareMicExciterData();
        UpdateClock();
      }

      centerFreq = oldCenterFreq;
      digitalWrite(RXTX, LOW);
      break;

    case CW_TRANSMIT_STRAIGHT_STATE:
      CWTransmit();
      break;

    case CW_TRANSMIT_KEYER_STATE:
      // turn on TX relay and initialize CW signal timer
      digitalWrite(RXTX, HIGH); // turn on TX relay
      cwTransmitTimer = millis();

      // start generating CW signal
      while(millis() - cwTransmitTimer <= cwTransmitDelay) {
        if(digitalRead(paddleDit) == LOW) {
          Dit();
          cwTransmitTimer = millis();

          // pause for one dit length
          IntraSpace();
        } else if(digitalRead(paddleDah) == LOW) {
          Dah();
          cwTransmitTimer = millis();

          // pause for one dit length
          IntraSpace();
        } else {
          CW_ExciterIQData(OFF);
        }
      }

      digitalWrite(RXTX, LOW);

      // delay a bit to allow play buffer to empty, otherwise
      // the remaining buffer will be played next time it's connected
      CWPause(50);
      break;

    case DATA_TRANSMIT_STATE:
      digitalWrite(RXTX, HIGH); // turn on TX relay
      ShowTransmitReceiveStatus();

      while(ft8PTT) {
        PrepareMicExciterData();
        UpdateClock();
        WSJTLoop(); // update ft8PTT
      }

      centerFreq = oldCenterFreq;
      digitalWrite(RXTX, LOW);
      break;

    default:
      break;
  }

#ifdef AUDIO_STATS
  if(lastState != radioState) {
    //EndAudioStats();
  }
  EndAudioStats();
#endif

  // save radio state for next loop
  lastState = radioState;

  UpdateClock();
  UpdateMemTempLoad();

  // slowly raise volume to avoid artifacts
  if(volSetting > 0) {
    if(audioVolume < volSetting) {
      audioVolume++;
      volumeChangeFlag = true;
    } else {
      volSetting = 0;
    }
  }

#ifdef T41_REMOTE_DISPLAY
  RemoteLoop();
#endif

#ifdef HOST_KEYBOARD_MOUSE_SUPPORT
  // just for testing
  if(elapsed_micros_idx_t > 200) {
    //PrintKeyboardBuffer();
  }

  if(keyerState == 1) {
    KeyerLoop();
  }
#endif

#ifdef NO_DISPLAY
  // need PC control without a display
  //T41ControlLoop();
#endif

#ifndef HOST_CAT_CONTROL_SUPPORT
  //T41ControlLoop();
#endif

#ifdef DEBUG_LOOP
  ExitLoop();
#endif

#ifdef NO_DISPLAY
  // along with the delay in ShowSpectrum this duplicates overall loop timing
  // with a display.  These are needed to regulate the flow of messages to the
  // PC control app.  These may not be needed if that app isn't used.
  delay(12);
#endif
}
