// v11 specific hardware source file

#include "..\SDT.h"

#include <SPI.h>
#include <RA8875.h>                    // https://github.com/mjs513/RA8875/tree/RA8875_t4

#ifdef USE_BPF_BOARD
#include <Wire.h>
#include <Adafruit_MCP23X17.h>
#endif

#include "..\AudioConfig.h"
#include "..\Button.h"
#include "..\calibrate.h"
#include "..\CW_Excite.h"
#include "..\CWProcessing.h"
#include "..\Display.h"
#include "displayRA8875\Display.h"
#include "..\EEPROM.h"
#include "..\Encoders.h"
#include "..\Exciter.h"
#include "..\hardware.h"
#include "..\Menu.h"
#include "..\MenuProc.h"
#include "..\Process.h"
#include "..\remote.h"
#include "..\Tune.h"
#include "..\Utility.h"

//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

uint16_t GPAB_state;
static Adafruit_MCP23X17 mcpBPF;

//------------
// Process.h

extern float32_t HP_DC_Filter_Coeffs2[];

float32_t HP_DC_Butter_state2[2] = { 0, 0 };
arm_biquad_cascade_df2T_instance_f32 s1_Receive2 = { 1, HP_DC_Butter_state2, HP_DC_Filter_Coeffs2 };

//------------
// T41_SDR.ino

int bandswitchPins[] = {
  FILTERPIN80M,  // 80M
  FILTERPIN40M,  // 40M
  FILTERPIN20M,  // 20M
  FILTERPIN15M,  // 17M
  FILTERPIN15M,  // 15M
  0,   // 12M  Note that 12M and 10M both use the 10M filter, which is always in (no relay).  KF5N September 27, 2023.
  0    // 10M
};

extern int oldCenterFreq;

// *** allow for v11 specific RA8875 code ***
extern RA8875 tft;

//-------------------------------------------------------------------------------------------------------------
// Forwards
//-------------------------------------------------------------------------------------------------------------

void EncodersInit();

void RFPowerFollowup();
void RFGainFollowup();
void FT8DoXmitCalibrate();

//-------------------------------------------------------------------------------------------------------------
// Code
//-------------------------------------------------------------------------------------------------------------

#ifdef USE_BPF_BOARD
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
#endif

//------------
// Exciter.cpp

/*****
  Purpose: Set the current band relay ON or OFF

  Parameter list:
    int state             OFF = 0, ON = 1
*****/
void SetBandRelay(int state) {
  // There are 4 physical relays.  Turn all of them off.
  for(int i = 0; i < 4; i = i + 1) {
    digitalWrite(bandswitchPins[i], LOW); // set ALL band relays low
  }

  // Set current band relay "on".  Ignore 12M and 10M.  15M and 17M use the same relay.
  if(t41.ActiveBand < BAND_12M) digitalWrite(bandswitchPins[t41.ActiveBand], state);
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
      //t41.TxPower = (float)GetEncoderValue(1, 20, t41.TxPower, 1, (char *)"Power: ");
      GetMenuValue(1, 20, (int*)&t41.TxPower, 1, "Power:", 200, NULL, NULL, &RFPowerFollowup);
      break;

    case 1: // Gain
      GetMenuValue(-60, 10, (int*)&t41.RFGain, 5, "Gain:", 200, NULL, NULL, &RFGainFollowup);
      break;
  }
}

/*****
  Purpose: Present the Calibrate options available and return the selection
*****/
FLASHMEM void CalibrateOptions() {
  static long long freqCorrectionFactorOld = freqCorrectionFactor;
  int val;
  //int32_t increment = 100L;

  tft.fillRect(SECONDARY_MENU_X, MENUS_Y, EACH_MENU_WIDTH + 30, CHAR_HEIGHT, RA8875_BLACK);

  if(calibrateItem < 0) {
    calibrateItem = secondaryMenuIndex;
  }

  switch(calibrateItem) {
    case 0:  // Frequency Cal - uses WWV
      //freqCorrectionFactor = GetEncoderValueLive(-200000, 200000, freqCorrectionFactor, increment, (char *)"Freq Cal: ");
      if(freqCorrectionFactor != freqCorrectionFactorOld) {
        //si5351.init(SI5351_CRYSTAL_LOAD_10PF, Si_5351_crystal, freqCorrectionFactor);
        //si5351.drive_strength(SI5351_CLK1, SI5351_DRIVE_8MA);
        //si5351.drive_strength(SI5351_CLK2, SI5351_DRIVE_8MA);
        SetSI5351FreqCorFactor(freqCorrectionFactor);
        SetFreq(t41.CenterFreq);
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
      if(keyPressedOn == 1 && t41.RadioMode == CW_MODE) {
        //================  CW Transmit Mode Straight Key ===========
        if(digitalRead(KEYER_DIT_INPUT_TIP) == LOW && t41.KeyType == 0) {  //Straight Key
          powerOutCW[t41.ActiveBand] = (-.0133 * t41.TxPower * t41.TxPower + .7884 * t41.TxPower + 4.5146) * CWPowerCalibrationFactor[t41.ActiveBand];
          CW_ExciterIQData();
          ShowTransmitReceiveStatus();
          SetFreq(t41.CenterFreq);                 //  AFP 10-02-22
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
      //CWPowerCalibrationFactor[t41.ActiveBand] = GetEncoderValueLive(-2.0, 2.0, CWPowerCalibrationFactor[t41.ActiveBand], 0.001, (char *)"CW PA Cal: ");
      powerOutCW[t41.ActiveBand] = (-.0133 * t41.TxPower * t41.TxPower + .7884 * t41.TxPower + 4.5146) * CWPowerCalibrationFactor[t41.ActiveBand];  // AFP 10-21-22
      val = ReadSelectedPushButton();
      if(val != BOGUS_PIN_READ) {        // Any button press??
        val = ProcessButtonPress(val);    // Use ladder value to get menu choice
        if(val == MENU_OPTION_SELECT) {  // Yep. Make a choice??
          tft.fillRect(SECONDARY_MENU_X, MENUS_Y, EACH_MENU_WIDTH + 35, CHAR_HEIGHT, RA8875_BLACK);
          //EEPROMData.CWPowerCalibrationFactor[t41.ActiveBand] = CWPowerCalibrationFactor[t41.ActiveBand];
          EEPROMWrite();
          calibrateItem = 5;
        }
      }
      break;

    case 2:  // SSB PA Cal
      //SSBPowerCalibrationFactor[t41.ActiveBand] = GetEncoderValueLive(-2.0, 2.0, SSBPowerCalibrationFactor[t41.ActiveBand], 0.001, (char *)"SSB PA Cal: ");
      //powerOutSSB[t41.ActiveBand] = (-.0133 * t41.TxPower * t41.TxPower + .7884 * t41.TxPower + 4.5146) * SSBPowerCalibrationFactor[t41.ActiveBand];  // AFP 10-21-22
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
    //  t41.RFGain is initialized to 0
    //dbm = dbm_calibration + bands[t41.ActiveBand].gainCorrection + (float32_t)attenuator + slope * log10f_fast(audioMaxSquaredAve) + cons - (float32_t)bands[t41.ActiveBand].rfGain * 1.5 - t41.RFGain;
    dbm = 29.0 + bands[t41.ActiveBand].gainCorrection + 0.0 + 10.0 * log10f_fast(audioMaxSquaredAve) + (-92.0) - (float32_t)bands[t41.ActiveBand].rfGain * 1.5 - t41.RFGain;
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

void InitHardware(int sampleRate) {
  // set up hardware specific Teensy pins that aren't handled elsewhere
  pinMode(FILTERPIN15M, OUTPUT);
  pinMode(FILTERPIN20M, OUTPUT);
  pinMode(FILTERPIN40M, OUTPUT);
  pinMode(FILTERPIN80M, OUTPUT);
  SetBandRelay(HIGH);

  pinMode(MUTE, OUTPUT);
  digitalWrite(MUTE, LOW);

  pinMode(BUSY_ANALOG_PIN, INPUT);

  InitSI5351();
  AudioSetup(sampleRate);

  InitFrontPanel();

#ifdef USE_BPF_BOARD
  SetupBPF();
#endif
}

void SoftResetHardware() {
  // encoder globals
  getEncoderValueFlag = false;
  resetTuningFlag = false;
  posFilterEncoder = 0;
  lastFilterEncoder = 0;
  filter_pos_BW = 0;
  last_filter_pos_BW = 0;
}

void ConfigRadioStateHardware() {
  switch(t41.RadioState) {
    case RECEIVE_STATE:
      break;

    case SSB_TRANSMIT_STATE:
    case DATA_TRANSMIT_STATE:
      oldCenterFreq = t41.CenterFreq;
      break;

    case CW_TRANSMIT_STRAIGHT_STATE:
    case CW_TRANSMIT_PADDLE_STATE:
    case CW_TRANSMIT_KEYER_STATE:
      break;

    default:
      break;
  }
}

void HardwareLoopStart() {
}
