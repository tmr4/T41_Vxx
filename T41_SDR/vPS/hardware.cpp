// v11 specific hardware source file

#include <Bounce.h>

#include "..\SDT.h"

#include "..\Button.h"
#include "..\ButtonProc.h"
#include "Calibrate.h"
#include "..\CW_Excite.h"
#include "..\CWProcessing.h"
#include "..\Display.h"
#include "..\EEPROM.h"
#include "..\Encoders.h"
#include "..\ft8.h"
#include "hardware.h"
#include "..\Menu.h"
#include "..\MenuProc.h"
#include "..\Process.h"
#include "..\remote.h"
#include "..\Tune.h"
#include "..\Utility.h"

//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

//------------
// Encoders.cpp

#ifdef PROJECTSYSTEM_ENCODER_1
Rotary volumeEncoder = Rotary(VOLUME_ENCODER_A, VOLUME_ENCODER_B);        // ( 2,  3)
Bounce encoderSwitch = Bounce(ENCODER_1_SWITCH, 10);  // 10 ms debounce
#endif

//------------
// Process.h

extern float32_t HP_DC_Filter_Coeffs2[];

float32_t HP_DC_Butter_state2[2] = { 0, 0 };
arm_biquad_cascade_df2T_instance_f32 s1_Receive2 = { 1, HP_DC_Butter_state2, HP_DC_Filter_Coeffs2 };

//------------
// T41_SDR.ino

extern long long oldCenterFreq;

#define PTT          37    // Transmit/Receive

//-------------------------------------------------------------------------------------------------------------
// Forwards
//-------------------------------------------------------------------------------------------------------------

void EncoderVolumeISR();

void RFPowerFollowup();
void RFGainFollowup();
void FT8DoXmitCalibrate();

//-------------------------------------------------------------------------------------------------------------
// Code
//-------------------------------------------------------------------------------------------------------------

//------------
// Button.cpp

/*****
  Purpose: Check for UI button press. If pressed, return the ADC value

  Parameter list:
    none

  Return value:
    int                   -1 if not valid push button, ADC value if valid
*****/
int ReadSelectedPushButton() {
  return 0;
}

int ProcessButtonPress(int valPin) {
  return 0;
}

//------------
// Encoders.cpp

// set up encoders
#ifdef PROJECTSYSTEM_ENCODER_1
void EncodersInit() {
  volumeEncoder.begin(true);
  attachInterrupt(digitalPinToInterrupt(VOLUME_ENCODER_A), EncoderVolumeISR, CHANGE);
  attachInterrupt(digitalPinToInterrupt(VOLUME_ENCODER_B), EncoderVolumeISR, CHANGE);

  // set up encoder switch debounce
  pinMode(ENCODER_1_SWITCH, INPUT_PULLUP);
}
#endif

/*****
  Purpose: Encoder volume control ISR
*****/
// why not FASTRUN
#ifdef PROJECTSYSTEM_ENCODER_1
void EncoderVolumeISR() {
  char result = 0;

  result = volumeEncoder.process();  // Read the encoder

  if(result == 0) {  // Nothing read
    return;
  }

  // TODO: check encoder setup as this is opposite T41
  switch(result) {
    case DIR_CW:  // Turned it clockwise, 16
      adjustVolEncoder = -1;
      break;

    case DIR_CCW:  // Turned it counter-clockwise
      adjustVolEncoder = 1;
      break;
  }

  if((calibrateItem >= 1) && (calibrateItem <= 3)) return;

  audioVolume += adjustVolEncoder;
  adjustVolEncoder = 0;

  if(audioVolume > MAX_AUDIO_VOLUME) {
    audioVolume = MAX_AUDIO_VOLUME;
  } else if(audioVolume < MIN_AUDIO_VOLUME) {
    audioVolume = MIN_AUDIO_VOLUME;
  }

  volumeChangeFlag = true; // flag needed for display update
}
#endif

void EncoderCenterTune() {
}

//------------
// MenuProc.cpp

/*****
  Purpose: Process RF options
*****/
FLASHMEM void RFOptions() {
  //  const char *rfOptions[] = { "Power level", "Gain", "Cancel" };
  switch(secondaryMenuIndex) {
    case 0: // Power Level
      //transmitPowerLevel = (float)GetEncoderValue(1, 20, transmitPowerLevel, 1, (char *)"Power: ");
      GetMenuValue(1, 20, &transmitPowerLevel, 1, "Power:", 200, NULL, NULL, &RFPowerFollowup);
      break;

    case 1: // Gain
      //rfGainAllBands = GetEncoderValue(-60, 10, rfGainAllBands, 5, (char *)"RF Gain dB: ");
      GetMenuValue(-60, 10, &rfGainAllBands, 5, "Gain:", 200, NULL, NULL, &RFGainFollowup);
      break;
  }
}

/*****
  Purpose: Present the Calibrate options available and return the selection
*****/
FLASHMEM void CalibrateOptions() {
  static long long freqCorrectionFactorOld = freqCorrectionFactor;
  int val;
  int32_t increment = 100L;

  tft.fillRect(SECONDARY_MENU_X, MENUS_Y, EACH_MENU_WIDTH + 30, CHAR_HEIGHT, RA8875_BLACK);

  if(calibrateItem < 0) {
    calibrateItem = secondaryMenuIndex;
  }

  switch(calibrateItem) {
    case 0:  // Frequency Cal - uses WWV
      freqCorrectionFactor = GetEncoderValueLive(-200000, 200000, freqCorrectionFactor, increment, (char *)"Freq Cal: ");
      if(freqCorrectionFactor != freqCorrectionFactorOld) {
        //si5351.init(SI5351_CRYSTAL_LOAD_10PF, Si_5351_crystal, freqCorrectionFactor);
        //si5351.drive_strength(SI5351_CLK1, SI5351_DRIVE_8MA);
        //si5351.drive_strength(SI5351_CLK2, SI5351_DRIVE_8MA);
        SetSI5351FreqCorFactor(freqCorrectionFactor);
        SetFreq();
        delay(10L);
        freqCorrectionFactorOld = freqCorrectionFactor;
      }
      val = ReadSelectedPushButton();
      if(val != BOGUS_PIN_READ) {        // Any button press??
        val = ProcessButtonPress(val);    // Use ladder value to get menu choice
        if(val == MENU_OPTION_SELECT) {  // Yep. Make a choice??
          tft.fillRect(SECONDARY_MENU_X, MENUS_Y, EACH_MENU_WIDTH + 35, CHAR_HEIGHT, RA8875_BLACK);
          EEPROMWrite();
          calibrateItem = 5;
        }
      }
      break;

    case 1:  // CW PA Cal
      if(keyPressedOn == 1 && radioMode == CW_MODE) {
        //================  CW Transmit Mode Straight Key ===========
        if(digitalRead(KEYER_DIT_INPUT_TIP) == LOW && keyType == 0) {  //Straight Key
          powerOutCW[currentBand] = (-.0133 * transmitPowerLevel * transmitPowerLevel + .7884 * transmitPowerLevel + 4.5146) * CWPowerCalibrationFactor[currentBand];
          CW_ExciterIQData();
          ShowTransmitReceiveStatus();
          SetFreq();                 //  AFP 10-02-22
          digitalWrite(MUTE, HIGH);  //   Mute Audio  (HIGH=Mute)
          //modeSelectInR.gain(0, 0);
          //modeSelectInL.gain(0, 0);
          //modeSelectInExR.gain(0, 0);
          //modeSelectOutL.gain(0, 0);
          //modeSelectOutR.gain(0, 0);
          //modeSelectOutExL.gain(0, 0);
          //modeSelectOutExR.gain(0, 0);
        }
      }
      CWPowerCalibrationFactor[currentBand] = GetEncoderValueLive(-2.0, 2.0, CWPowerCalibrationFactor[currentBand], 0.001, (char *)"CW PA Cal: ");
      powerOutCW[currentBand] = (-.0133 * transmitPowerLevel * transmitPowerLevel + .7884 * transmitPowerLevel + 4.5146) * CWPowerCalibrationFactor[currentBand];  // AFP 10-21-22
      val = ReadSelectedPushButton();
      if(val != BOGUS_PIN_READ) {        // Any button press??
        val = ProcessButtonPress(val);    // Use ladder value to get menu choice
        if(val == MENU_OPTION_SELECT) {  // Yep. Make a choice??
          tft.fillRect(SECONDARY_MENU_X, MENUS_Y, EACH_MENU_WIDTH + 35, CHAR_HEIGHT, RA8875_BLACK);
          EEPROMData.CWPowerCalibrationFactor[currentBand] = CWPowerCalibrationFactor[currentBand];
          EEPROMWrite();
          calibrateItem = 5;
        }
      }
      break;

    case 2:  // SSB PA Cal
      //SSBPowerCalibrationFactor[currentBand] = GetEncoderValueLive(-2.0, 2.0, SSBPowerCalibrationFactor[currentBand], 0.001, (char *)"SSB PA Cal: ");
      //powerOutSSB[currentBand] = (-.0133 * transmitPowerLevel * transmitPowerLevel + .7884 * transmitPowerLevel + 4.5146) * SSBPowerCalibrationFactor[currentBand];  // AFP 10-21-22
      //val = ReadSelectedPushButton();
      //if(val != BOGUS_PIN_READ) {        // Any button press??
      //  val = ProcessButtonPress(val);    // Use ladder value to get menu choice
      //  if(val == MENU_OPTION_SELECT) {  // Yep. Make a choice??
      //    tft.fillRect(SECONDARY_MENU_X, MENUS_Y, EACH_MENU_WIDTH + 35, CHAR_HEIGHT, RA8875_BLACK);
      //    EEPROMWrite();
      //    calibrateItem = 5;
      //  }
      //}
      calibrateItem = -1;
      break;

    case 3: // IQ Cal - Gain and Phase
      CalibrateIQ();
      calibrateItem = -1;
      break;

    case 4: // Two Tone
      calibrateItem = -1;
      break;

    case 5: // cancel wrap up calibration
      calibrateItem = -1;
      break;

    default:  // Cancelled choice
      break;
  }
}

//------------
// Process.cpp

void RemoveDCBias() {
  arm_biquad_cascade_df2T_f32(&s1_Receive2, audioBufferL, audioBufferL, 2048);
  arm_biquad_cascade_df2T_f32(&s1_Receive2, audioBufferR, audioBufferR, 2048);
}

float32_t CalcSignalStrength() {
  float32_t dbm = -131.0;
  //float32_t dbm_calibration = 22.0;
  //float32_t dbm_calibration = 25.0; // calibrated with AD3 (1mW -73dB external attenuation, 223.6mVrms @7.047MHz; see "Wavegen for RF in - S9 - 1mW with 73dB external atten.dwf3work")
  //const float32_t slope = 10.0;
  //const float32_t cons = -92.0;
  //const int attenuator = 0;

  // prevent NAN dBm
  if(audioMaxSquaredAve > 0.0) {
    // dbm_calibration set to 25; gainCorrection is a value between -2 and +6 to compensate the frequency dependant pre-Amp gain
    // attenuator is 0 and could be set in a future HW revision; RFgain is initialized to 1 in the bands[] init in SDT.ino; cons=-92; slope=10
    //  rfGainAllBands is initialized to 0
    //dbm = dbm_calibration + bands[currentBand].gainCorrection + (float32_t)attenuator + slope * log10f_fast(audioMaxSquaredAve) + cons - (float32_t)bands[currentBand].RFgain * 1.5 - rfGainAllBands;
    dbm = 29.0 + bands[currentBand].gainCorrection + 0.0 + 10.0 * log10f_fast(audioMaxSquaredAve) + (-92.0) - (float32_t)bands[currentBand].RFgain * 1.5 - rfGainAllBands;
  } else {

    // reset audioMaxSquaredAve to a small value
    // with default parameters and audioMaxSquaredAve = 1.778e-6, dBm = -131
    audioMaxSquaredAve = 0.0;
    //Serial.println("dBm is NAN");
  }

  return dbm;
}

//------------
// T41_SDR.ino

void InitHardware() {
  // set up hardware specific Teensy pins that aren't handled elsewhere
  pinMode(FILTERPIN15M, OUTPUT);
  pinMode(FILTERPIN20M, OUTPUT);
  pinMode(FILTERPIN40M, OUTPUT);
  pinMode(FILTERPIN80M, OUTPUT);

  pinMode(MUTE, OUTPUT);
  digitalWrite(MUTE, LOW);

  pinMode(BUSY_ANALOG_PIN, INPUT);

#if defined(FOURSQRP_FRONTPANEL)
  EnableButtonInterrupts();
  EncodersInit();
#else
  #ifdef PROJECTSYSTEM_ENCODER_1
  EncodersInit();
  #endif
#endif
}

void SoftResetHardware() {
  // encoder globals
  getEncoderValueFlag = false;
  volumeChangeFlag = false;
  resetTuningFlag = false;
  fineTuneFlag = false;
  posFilterEncoder = 0;
  lastFilterEncoder = 1; // force initial update
  filter_pos_BW = 0;
  last_filter_pos_BW = 0;
}

void ConfigRadioStateHardware() {
  switch(radioState) {
    case SSB_RECEIVE_STATE:
      break;

    case SSB_TRANSMIT_STATE:
    case DATA_TRANSMIT_STATE:
      oldCenterFreq = centerFreq;
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
      break;

    default:
      break;
  }
}

void HardwareLoopStart() {
  // poll encoder switch
  if(encoderSwitch.update() && encoderSwitch.fallingEdge()) {
    if(radioMode == DATA_MODE) {
      // load wave file and begin decoding internally if successful
      ExecuteButtonPress(16);
    } else {
      // switch to data mode and internal FT8 mode
      currentDataMode = DEMOD_FT8_DECODE;
      ChangeMode(DATA_MODE);

      // load wave file and begin decoding internally if successful
      ExecuteButtonPress(16);
    }
  }
}
