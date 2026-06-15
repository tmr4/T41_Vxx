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
#include "hardware.h"
#include "Menu.h"
#include "Noise.h"
#include "Process.h"
#include "Tune.h"
#include "Utility.h"

// special features
#include "debug.h"
#include "keyboard.h"
#include "keyer.h"
#include "ft8.h"
#include "mouse.h"
#include "remoteDisplay.h"
#include "t41Beacon.h"

// *** need to pull what we want from these ***
//#include "fir_cmsis_5k.h"
//#include "fir_alt.h"

#include "catControl.h"
#include "telemetry.h"
#include "USBManager.h"
#include "connectManager.h"

//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

extern CatControl catControl;
extern CatControl wsjtControl;

#if RADIO_ROLE == 7
ConnectManager connectManager;
#elif RADIO_ROLE == 6
ConnectManager connectManager(DEVICE_ROLE_REMOTE);
#endif

extern bool beaconFlag;
extern bool iqSyncSearch;

float32_t DMAMEM audioBufferL[2048];
float32_t DMAMEM audioBufferR[2048];
float32_t DMAMEM audioBufferL_EX[2048];
float32_t DMAMEM audioBufferR_EX[2048];
float32_t DMAMEM audioBufferTemp[2048];

/*
typedef struct {
  long freq;        // Current frequency in Hz
  long fBandLow;    // Lower band edge
  long fBandHigh;   // Upper band edge
  const char* name; // name of band
  int demod;        // standard SSB/CW demodulation mode
  int fHiCut;
  int fLoCut;
  int rfGain;
  long calFreq; // receive IQ calibration frequency, set to 0 to skip calibration of a specific band
  float32_t gainCorrection; // is hardware dependent and has to be calibrated ONCE and hardcoded in the band table
  int agcThresh;
  int16_t pixelOffset;
} band;
*/

// gainCorrection used in signal strength calculation
// set with signal from AD3 (1mW -73dB external attenuation, 223.6mVrms @ 1kHz w/ default freq for band; see "Wavegen for RF in - S9 - 1mW with 73dB external atten.dwf3work")
band bands[NUMBER_OF_BANDS] = {
//  freq      band low   band hi   name    standard     low  high   Gain  calFreq      gain                    AGC   pixel
//                                         demodulation  filter                       correct                       offset
//  freq      fBandLow   fBandHigh name    demod      fLoCut fHiCut rfGain             gainCorrection
    3700000,  3500000,   4000000,  "80M",  DEMOD_LSB,   200, 3000,  1,    3750000,     GAIN_CORRECTION_80M,    20,    20,
    7150000,  7000000,   7300000,  "40M",  DEMOD_LSB,   200, 3000,  1,    7150000,     GAIN_CORRECTION_40M,    20,    20,
    14200000, 14000000, 14350000,  "20M",  DEMOD_USB,   200, 3000,  1,    14175000,    GAIN_CORRECTION_20M,    20,    20,
    18100000, 18068000, 18168000,  "17M",  DEMOD_USB,   200, 3000,  1,    18118000,    GAIN_CORRECTION_17M,    20,    20,
    21200000, 21000000, 21450000,  "15M",  DEMOD_USB,   200, 3000,  1,    21225000,    GAIN_CORRECTION_15M,    20,    20,
    24920000, 24890000, 24990000,  "12M",  DEMOD_USB,   200, 3000,  1,    24940000,    GAIN_CORRECTION_12M,    20,    20,
    28350000, 28000000, 29700000,  "10M",  DEMOD_USB,   200, 3000,  1,           0,    GAIN_CORRECTION_10M,    20,    20 // auto calibration not performed on this band
};

int oldCenterFreq;

//-------------------------------------------------------------------------------------------------------------
// Forwards
//-------------------------------------------------------------------------------------------------------------

int SetI2SFreq(int freq);

//-------------------------------------------------------------------------------------------------------------
// Code
//-------------------------------------------------------------------------------------------------------------

FLASHMEM void InitializeDataArrays(int sampleRate) {
  InitFFTArrays();

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
  InitFIRFilters(sampleRate);
  InitZoomFFTFilter(sampleRate);
  InitSpectralNoiseReduction();
  InitLMSNoiseReduction();

  // this needs to come after above
  InitAMDemodBiquadFilter(sampleRate);

  // prepare 750Hz signal buffer
  GenSineToneBuffers(8);
}

FLASHMEM void Splash() {
  // 50 char max for 800x480 display with font scale = 1:
  //                     "          1         2         3         4"
  //                     "01234567890123456789012345678901234567890123456789";
  const char*line1Txt = "T41-EP";
  const char*line2Txt = "Version: "; // + VERSION
  const char*line3Txt = "By: Terrance Robertson, KN6ZDE";
  const char*line4Txt = "Based on design by: Al Peter, AC8GY and Jack Purdum, W8TEE";
  const char*line5Txt = "";
  //const char*line6Txt = "Property of:"; // line 7 MY_CALL

  ShowSplash(line1Txt, line2Txt, line3Txt, line4Txt, line5Txt);
}

/*****
  Purpose: perform a soft reset of the radio
              This resets the user modifiable radio settings to the startup state
*****/
FLASHMEM void SoftReset() {
  if(LOAD_VARS_FROM_EEPROM) {
    LoadOpVarsFromEEPROM(LOAD_VARS_FROM_EEPROM);
  } else {
    t41.SetPropertyDefaults();
  }

  splitVFO = false;
  SoftResetHardware();

  SetKeyPowerUp();  // Use t41.KeyType and paddleFlip to configure key GPIs
  SetDitLength(t41.CurrentWPM);
  SetTransmitDitLength();
  menuEncoderMove = 0;
  fineTuneEncoderMove = 0L;

  mainMenuIndex = 0;             // Changed from middle to first. Do Menu Down to get to Calibrate quickly
  secondaryMenuIndex = -1;       // -1 means haven't determined secondary menu
  menuStatus = NO_MENUS_ACTIVE;  // Blank menu field

  t41.DemodMode = bands[t41.ActiveBand].demod;

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

  t41.NCOFreq = 0;
  ResetTuning();
  ShowFrequency(true);
  CalcAudioFilters();
}

// *** for testing ***
//extern "C" uint8_t external_psram_size;

void MaxStackUseSetup();

FLASHMEM void setup() {
  int sampleRate = 192000.0;

  Serial.begin(9600);
  /* check for CrashReport stored from previous run */
  if (CrashReport) {
    while (!Serial && millis() < 10000) ; /* wait up to 10 sec */
    /* print info (hope Serial Monitor windows is open) */
    Serial.print(CrashReport);
  }
  //delay(1000);
  // *** TODO: comment when no longer monitoring stack usage ***
  MaxStackUseSetup();

  // Check for PSRAM chip(s) installed
  //uint8_t size = external_psram_size;
  //if (size == 0) {
  //  Serial.println("No PSRAM Installed");
  //} else {
  //  Serial.printf("PSRAM Memory Size = %d Mbyte\n", size);
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

  // SD card is required for normal T41 operations
  // *** TODO: reconsider this ***
  //if(CheckDataFileEEPROM() == 0) { // *** requires SDEEPROMData.txt on SD card ***
  while(InitializeSDCard() == 0) {
    Debug("No SD card");
    //Serial.println("no sd");
    ShowNoSD();
    for(int i = 0; i < 10; i++) {
      ShowDot();
      delay(500L);
    }
  }

  //SaveAnalogSwitchValues();

  ClearScreen();
  EEPROMStartup();

#ifdef DEBUG
  EEPROMShow();
#endif

  delay(100L);

  InitializeDataArrays(sampleRate);

  InitHardware(sampleRate);
  SoftReset();

#if HOST_KEYBOARD_MOUSE_SUPPORT
  KeyboardSetup();
  MouseInit();
#endif

  USBManager::begin();

  //memCheck = true;
  PrimeMallInfo();

  //T41BeaconSetup();

#if RADIO_ROLE > 0
#if T41_WSJT_CAT_AUDIO
  connectManager.begin(&catControl, &iqStreamEthernet, &iqStreamUSB);
#else
  connectManager.begin(&catControl, &iqStreamEthernet, &iqStreamUSB);
#endif

#endif

  KeyerSetup(); // testing only

  // initialize Teensy temperature monitor
  // temp_check_frequency = 0x03U;  //updates the temp value at a RTC/3 clock rate
  // 0xFFFF determines a 2 second sample rate period
  //initTempMon(temp_check_frequency, lowAlarmTemp, highAlarmTemp, panicAlarmTemp);
  initTempMon(0x03U, 25U, 85U, 90U);  // 85U = 42 degrees C?
  // this starts the measurements
  TEMPMON_TEMPSENSE0 |= 0x2U;

#ifdef PROFILER_ACTIVE
  pinMode(PROFILER_PROCESS_RX, OUTPUT);
  digitalWrite(PROFILER_PROCESS_RX, LOW);
  pinMode(PROFILER_MAINLOOP, OUTPUT);
  digitalWrite(PROFILER_MAINLOOP, LOW);
  pinMode(PROFILER_DRAW, OUTPUT);
  digitalWrite(PROFILER_DRAW, LOW);
  pinMode(PROFILER_ENTRY, OUTPUT);
  digitalWrite(PROFILER_ENTRY, LOW);
  pinMode(PROFILER_PROCESS_FRAME, OUTPUT);
  digitalWrite(PROFILER_PROCESS_FRAME, LOW);
  pinMode(PROFILER_RX_TX, OUTPUT);
  digitalWrite(PROFILER_RX_TX, LOW);
  pinMode(PROFILER_DECODE_FT8, OUTPUT);
  digitalWrite(PROFILER_DECODE_FT8, LOW);
  pinMode(PROFILER_OTHER, OUTPUT);
  digitalWrite(PROFILER_OTHER, LOW);
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

/*
void ConfigRadioState() {
  ConfigRadioStateHardware();

  switch(t41.RadioState) {
    case RECEIVE_STATE:
      break;

    case SSB_TRANSMIT_STATE:
      break;

    case CW_TRANSMIT_STRAIGHT_STATE:
    case CW_TRANSMIT_PADDLE_STATE:
    case CW_TRANSMIT_KEYER_STATE:
      break;

    default:
      break;
  }
}
*/

long loopTimeSum, loopCount;

FASTRUN void loop() {
  elapsedMillis loopTime;
  int pushButtonSwitchIndex = -1;
  int valPin;
  bool reconfigureFlag = t41.RadioState == RECONFIGURE_STATE;
  static int lastState = -1;

  // *** can't use set/reset here as it can be hard to catch with a quick loop ***
  //SETPROFILEPIN(PROFILER_MAINLOOP);
  TOGGLEPROFILEPIN(PROFILER_MAINLOOP);

  // 1. run routines that may change the state of the radio
  HardwareLoopStart();

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

#if HOST_KEYBOARD_MOUSE_SUPPORT
  if(keyerState == 1) {
    KeyerLoop();
  }
#endif

  ProcessControls(); // *** needed for any processes that skips YieldToProcess ***

  // check for UI button press and process accordingly
  valPin = ReadSelectedPushButton();
  if(valPin != BOGUS_PIN_READ) {
    pushButtonSwitchIndex = ProcessButtonPress(valPin);
    ExecuteButtonPress(pushButtonSwitchIndex);
  }

  // 2. state detection
  switch(t41.RadioMode) {
    case SSB_MODE:
      if(t41.RadioMode == SSB_MODE && digitalRead(PTT) == HIGH) {
        t41.RadioState = RECEIVE_STATE;
      } else {
        t41.RadioState = SSB_TRANSMIT_STATE;
      }
      break;
    case CW_MODE:
      if(cwKeyerPTT) {
        t41.RadioState = CW_TRANSMIT_KEYER_STATE;
      } else if(beaconFlag) {
        t41.RadioState = BEACON_STATE;
      } else if((digitalRead(t41.PaddleDit) == HIGH) && (digitalRead(t41.PaddleDah) == HIGH)) {
        t41.RadioState = RECEIVE_STATE;
      } else if((digitalRead(t41.PaddleDit) == LOW) && (t41.KeyType == 0)) {
        t41.RadioState = CW_TRANSMIT_STRAIGHT_STATE;
      } else if((keyPressedOn == 1) && (t41.KeyType == 1)) {
        t41.RadioState = CW_TRANSMIT_PADDLE_STATE;
        keyPressedOn = 0;
      }
      break;
    case DSB_MODE:
      t41.RadioState = RECEIVE_STATE;
      break;
    case DATA_MODE:
      //Serial.print("ft8PTT: "); Serial.println(ft8PTT);
      if(ft8PTT) {
        t41.RadioState = DATA_TRANSMIT_STATE;
      } else {
        t41.RadioState = RECEIVE_STATE;
      }
      break;
  }

  // 3. configure radio for current state
  if(t41.RadioState != lastState || reconfigureFlag) {
    // cleanup last state
    switch(lastState) {
      case RECEIVE_STATE:
        break;

      case DATA_TRANSMIT_STATE:
        digitalWrite(RXTX, LOW); // turn off TX relay
        break;

      case CALIBRATE_TRANSMIT_STATE:
        break;

      default:
        break;
    }

    ConfigAudioState(t41.RadioState);
    ConfigRadioStateHardware();
    SetFreq(t41.CenterFreq);  // Update frequencies if the radio state has changed
    ShowTransmitReceiveStatus();
  }

  // save radio state for next loop
  lastState = t41.RadioState;

  // 4. process radio state
  switch(t41.RadioState) {
    case RECEIVE_STATE:
      switch(displayState) {
        case DISPLAY_T41:
          UpdateLiveDisplayAreas();
          break;

        case DISPLAY_T41_FT8_DECODE:
          FT8DecoderLoop();
          break;

        case DISPLAY_BEACON_MONITOR:
        default:
        // process control and IQ signals without updating display
        // (other events may still update display, clock for example)
        // *** TODO: many control tasks still update screen.  Fix this. ***
        YieldToProcess();
        break;
      }
      break;

    case SSB_TRANSMIT_STATE:
      digitalWrite(RXTX, HIGH); // turn on TX relay

      while(digitalRead(PTT) == LOW) {
        PrepareMicExciterData();
        UpdateClock();
      }

      t41.CenterFreq = oldCenterFreq;
      digitalWrite(RXTX, LOW);
      break;

    case CW_TRANSMIT_STRAIGHT_STATE:
      CWTransmit();
      break;

    case CW_TRANSMIT_PADDLE_STATE:
      CWTransmitPaddle();
      break;

    case CW_TRANSMIT_KEYER_STATE:
      CWTransmitMessage();
      break;

    case DATA_TRANSMIT_STATE:
      digitalWrite(RXTX, HIGH); // turn on TX relay
      ShowTransmitReceiveStatus();

      while(ft8PTT) {
        static int i = 0;

        switch(t41.DemodMode) {
          case DEMOD_FT8:
              PrepareMicExciterData();
              #if T41_WSJT_CAT_AUDIO
              wsjtControl.update(); // update ft8PTT
              #endif
            break;

          case DEMOD_FT8_INTERNAL:
            TOGGLEPROFILEPIN(PROFILER_MAINLOOP);
            // transmit FT8 signal about ~10ms at a time
            // total transmit time = 12.64 sec or (79 symbols * 0.16 sec/symbol)
            // this is 151680 samples (12.64 sec * 12000 samples/sec)
            // play one buffer past msg to flush output buffer
            // without this about 5ms of decay pulse will remain
            // to play at next interval (even with pause below)
            if(ft8TxSignalBuf != NULL && i < 151680 + 128) {
              TOGGLEPROFILEPIN(PROFILER_OTHER);
              PrepareFT8ExciterIQData(ft8TxSignalBuf + i);
              i += 128;
            } else {
              i = 0;
              ft8PTT = false;
              ft8TxSignalBuf = NULL;
            }
            break;
        }

        UpdateClock();
      }

      t41.CenterFreq = oldCenterFreq;
      digitalWrite(RXTX, LOW);

      // delay a bit to allow play buffer to empty, otherwise
      // the remaining buffer will be played next time it's connected
      //CWPause(25); // 28ms plays on restart
      CWPause(50); // 5ms plays on restart, but it's the same w/ higher delay, first transmit doesn't have this
      break;

    default:
      break;
  }

  // 5. wrap up loop with other housekeeping
#ifdef AUDIO_STATS
  EndAudioStats();
#endif

#ifdef T41_REMOTE_DISPLAY
  RemoteLoop();
#endif
  loopTimeSum += loopTime;
  ++loopCount;
}
