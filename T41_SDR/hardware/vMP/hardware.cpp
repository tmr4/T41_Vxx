// Mini Platform specific hardware source file

#include <Bounce.h>

#include "..\SDT.h"

#include "..\AudioConfig.h"
#include "..\Button.h"
#include "..\ButtonProc.h"
#include "..\CW_Excite.h"
#include "..\CWProcessing.h"
#include "..\Display.h"
#include "..\EEPROM.h"
#include "..\Encoders.h"
#include "..\ft8.h"
#include "..\Menu.h"
#include "..\MenuProc.h"
#include "..\Process.h"
#include "..\remote.h"
#include "..\Tune.h"
#include "..\Utility.h"

//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

extern Bounce encoderSwitch;
extern Bounce encoder2Switch;

//------------
// Process.h

extern float32_t HP_DC_Filter_Coeffs2[];

float32_t HP_DC_Butter_state2[2] = { 0, 0 };
arm_biquad_cascade_df2T_instance_f32 s1_Receive2 = { 1, HP_DC_Butter_state2, HP_DC_Filter_Coeffs2 };

//------------
// T41_SDR.ino

extern int oldCenterFreq;

//-------------------------------------------------------------------------------------------------------------
// Forwards
//-------------------------------------------------------------------------------------------------------------

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
  return -1;
}

int ProcessButtonPress(int valPin) {
  return 0;
}

//------------
// Encoders.cpp

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
  //static long long freqCorrectionFactorOld = freqCorrectionFactor;
  //int val;
  //int32_t increment = 100L;

  if(calibrateItem < 0) {
    calibrateItem = secondaryMenuIndex;
  }

  switch(calibrateItem) {
    case 0:  // Frequency Cal - uses WWV
      break;

    case 1:  // CW PA Cal
      break;

    case 2:  // SSB PA Cal
      break;

    case 3: // IQ Cal - Gain and Phase
      break;

    case 4: // Two Tone
      break;

    case 5: // cancel wrap up calibration
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

float CalcSignalStrength() {
  float32_t dbm = -131.0;
  //float32_t dbm_calibration = 22.0;
  //float32_t dbm_calibration = 25.0; // calibrated with AD3 (1mW -73dB external attenuation, 223.6mVrms @7.047MHz; see "Wavegen for RF in - S9 - 1mW with 73dB external atten.dwf3work")
  //const float32_t slope = 10.0;
  //const float32_t cons = -92.0;
  //const int attenuator = 0;

  // prevent NAN dBm
  if(audioMaxSquaredAve > 0.0) {
    // dbm_calibration set to 25; gainCorrection is a value between -2 and +6 to compensate the frequency dependant pre-Amp gain
    // attenuator is 0 and could be set in a future HW revision; rfGain is initialized to 1 in the bands[] init in SDT.ino; cons=-92; slope=10
    //  rfGainAllBands is initialized to 0
    //dbm = dbm_calibration + bands[currentBand].gainCorrection + (float32_t)attenuator + slope * log10f_fast(audioMaxSquaredAve) + cons - (float32_t)bands[currentBand].rfGain * 1.5 - rfGainAllBands;
    dbm = 29.0 + bands[currentBand].gainCorrection + 0.0 + 10.0 * log10f_fast(audioMaxSquaredAve) + (-92.0) - (float32_t)bands[currentBand].rfGain * 1.5 - rfGainAllBands;
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
  pinMode(MUTE, OUTPUT);
  digitalWrite(MUTE, LOW);

  pinMode(BUSY_ANALOG_PIN, INPUT);

  AudioSetup(false);

  EncodersInit();
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
      oldCenterFreq = t41.CenterFreq;
      break;

    case CW_RECEIVE_STATE:
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
}
