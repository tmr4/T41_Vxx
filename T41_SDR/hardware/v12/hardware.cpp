// v12 specific hardware source file

#include "..\SDT.h"

#include "..\AudioConfig.h"
#include "..\Button.h"
#include "..\CW_Excite.h"
#include "..\Display.h"
#include "..\EEPROM.h"
#include "..\Encoders.h"
#include "..\hardware.h"
#include "..\Menu.h"
#include "..\MenuProc.h"
#include "..\Process.h"
#include "RF_Control.h"
#include "..\Tune.h"
#include "..\t41Control.h"
#include "..\Utility.h"

//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

//extern uint8_t twinpeaks_tested;
//extern uint8_t write_analog_gain;

//------------
// Display.cpp

int attenuator = 0;

//------------
// T41_SDR.ino

extern int oldCenterFreq;

//------------
// Utility.cpp

// used in PrepareMicExciterData for two tone test
float32_t sinBuffer4[256];
float32_t sinBuffer5[256];

I2C bit_results;

//const float CWToneOffsetsHz[] = {0, 562.5, 656.5, 750.0, 843.75 };  // these correspond to the definitions in CWProcessing.cpp

//-------------------------------------------------------------------------------------------------------------
// Forwards
//-------------------------------------------------------------------------------------------------------------

void EncodersInit();

void RFPowerFollowup();
void RFGainFollowup();
void T41ControlSendCmd(char *cmd);

void CalibrateReceiveIQ();
void CalibrateTransmitIQ();

//-------------------------------------------------------------------------------------------------------------
// Code
//-------------------------------------------------------------------------------------------------------------

//------------
// Display.cpp

/*****
  Purpose: ShowAnalogGain()


  Return value;
    void
*****/
void ShowAnalogGain() {
  // *** currently write_analog_gain is always 0 ***
/*
  static uint8_t RF_gain_old = 0;
  static uint8_t RF_att_old = 0;
  const uint16_t col = RA8875_GREEN;
  // Note that attenuator = attenuation * 2. It is also not used in the plot below.
  int attenuator = 0;

  attenuator = currentRF_InAtten;

  if((((bands[t41.ActiveBand].rfGain != RF_gain_old) || (attenuator != RF_att_old)) && twinpeaks_tested == 1) || write_analog_gain) {
    tft.setFontScale((enum RA8875tsize)0);
    tft.setCursor(TIME_X - 40, TIME_Y + 26); // *** TODO: evaluate position ***
    tft.print((float)(RF_gain_old * 1.5));
    tft.setTextColor(col);
    tft.print("dB -");

    tft.setTextColor(RA8875_BLACK);
    tft.print("dB -");
    tft.setTextColor(RA8875_BLACK);
    tft.print("dB");
    tft.setTextColor(col);
    tft.print("dB = ");

    tft.setFontScale((enum RA8875tsize)0);

    tft.setTextColor(RA8875_BLACK);
    tft.print("dB");
    tft.setTextColor(RA8875_WHITE);
    tft.print("dB");
    RF_gain_old = bands[t41.ActiveBand].rfGain;
    RF_att_old = attenuator;
    //write_analog_gain = 0;
  }
*/
}

//------------
// MenuProc.cpp

FLASHMEM void RFInAttenFollowup() {
  SetRF_InAtten(currentRF_InAtten);
  RAtten[t41.ActiveBand] = currentRF_InAtten;

  ShowAnalogGain();

  // *** TODO: set to EEPROM ***
  //EEPROMData.rfGainAllBands = rfGainAllBands;
  EEPROMWrite();
}

FLASHMEM void SetRFInAttenValue() {
  SetRF_InAtten(currentRF_InAtten);
  //RAtten[t41.ActiveBand] = currentRF_InAtten;
}

FLASHMEM void SSBRFOutAttenFollowup() {
  SetRF_OutAtten(currentRF_OutAtten);
  XAttenSSB[t41.ActiveBand] = currentRF_OutAtten;

  // *** TODO: set to EEPROM ***
  //EEPROMData.rfGainAllBands = rfGainAllBands;
  EEPROMWrite();
}

FLASHMEM void CWRFOutAttenFollowup() {
  SetRF_OutAtten(currentRF_OutAtten);
  XAttenCW[t41.ActiveBand] = currentRF_OutAtten;

  // *** TODO: set to EEPROM ***
  //EEPROMData.rfGainAllBands = rfGainAllBands;
  EEPROMWrite();
}

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

    case 2:  // RF In Atten
      GetMenuValue(0, 63, &currentRF_InAtten, 1, "In Att:", 200, NULL, &SetRFInAttenValue, &RFInAttenFollowup);
      //SetRF_InAtten(currentRF_InAtten);
      break;

    case 3:  // SSB RF Out Atten
      GetMenuValue(0, 63, &currentRF_OutAtten, 1, "SSB Att:", 200, NULL, NULL, &SSBRFOutAttenFollowup);
      //powerOutSSB[t41.ActiveBand] = currentRF_OutAtten;
      //SetRF_OutAtten(currentRF_OutAtten);
      break;

    case 4:  // CW RF Out Atten
      GetMenuValue(0, 63, &currentRF_OutAtten, 1, "CW Att:", 200, NULL, NULL, &CWRFOutAttenFollowup);
      break;
  }
}

/*****
  Purpose: Present the Calibrate options available and return the selection
*****/
FLASHMEM void CalibrateOptions() {
  int val;
  //int32_t increment = 100L;

  tft.fillRect(SECONDARY_MENU_X, MENUS_Y, EACH_MENU_WIDTH + 30, CHAR_HEIGHT, RA8875_BLACK);

  if(calibrateItem < 0) {
    calibrateItem = secondaryMenuIndex;
  }

  switch(calibrateItem) {
    case 0: // Calibrate Frequency  - uses WWV
      ResetTuning();
      CalibrateFrequency();
      calibrateItem = -1;
      break;

    case 1: // IQ Receive Cal - Gain and Phase
      CalibrateReceiveIQ();
      calibrateItem = -1;
      break;

    case 2: // IQ Transmit Cal - Gain and Phase
      CalibrateTransmitIQ();
      calibrateItem = -1;
      break;

    case 3:  // Two tone
      TwoToneTest();
      calibrateItem = -1;
      break;

    case 4:  // CW PA Cal
      if(keyPressedOn == 1 && t41.RadioMode == CW_MODE) {
        //================  CW Transmit Mode Straight Key ===========
        if(digitalRead(KEYER_DIT_INPUT_TIP) == LOW && keyType == 0) {  //Straight Key
          powerOutCW[t41.ActiveBand] = (-.0133 * transmitPowerLevel * transmitPowerLevel + .7884 * transmitPowerLevel + 4.5146) * CWPowerCalibrationFactor[t41.ActiveBand];
          CW_ExciterIQData();
          ShowTransmitReceiveStatus();
          SetFreq(t41.CenterFreq);
          //digitalWrite(MUTE, HIGH);  //   Mute Audio  (HIGH=Mute)
          //modeSelectInR.gain(0, 0);
          //modeSelectInL.gain(0, 0);
          //modeSelectInExR.gain(0, 0);
          //modeSelectOutL.gain(0, 0);
          //modeSelectOutR.gain(0, 0);
          //modeSelectOutExL.gain(0, 0);
          //modeSelectOutExR.gain(0, 0);
        }
      }
      CWPowerCalibrationFactor[t41.ActiveBand] = GetEncoderValueLive(-2.0, 2.0, CWPowerCalibrationFactor[t41.ActiveBand], 0.001, (char *)"CW PA Cal: ");
      powerOutCW[t41.ActiveBand] = (-.0133 * transmitPowerLevel * transmitPowerLevel + .7884 * transmitPowerLevel + 4.5146) * CWPowerCalibrationFactor[t41.ActiveBand];  // AFP 10-21-22
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

    case 5:  // SSB PA Cal
      SSBPowerCalibrationFactor[t41.ActiveBand] = GetEncoderValueLive(-2.0, 2.0, SSBPowerCalibrationFactor[t41.ActiveBand], 0.001, (char *)"SSB PA Cal: ");
      powerOutSSB[t41.ActiveBand] = (-.0133 * transmitPowerLevel * transmitPowerLevel + .7884 * transmitPowerLevel + 4.5146) * SSBPowerCalibrationFactor[t41.ActiveBand];  // AFP 10-21-22
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

    case 6: // wrap up calibration
      //EraseMenus();
      //RedrawDisplayScreen();
      //DrawBandwidthBar();
      //ShowFrequency();
      //ShowOperatingStats();
      calibrateItem = -1;
      //modeSelectOutExL.gain(0, 0);
      //modeSelectOutExR.gain(0, 0);
      break;

    default:  // Cancelled choice
      break;
  }
}

//------------
// Process.cpp

void RemoveDCBias() {}

float CalcSignalStrength() {
  float dbm = -131.0;
  //float32_t dbm_calibration = 22.0;
  //float32_t dbm_calibration = 31.0; // calibrated with AD3 (1mW -73dB external attenuation, 223.6mVrms @7.047MHz; see "Wavegen for RF in - S9 - 1mW with 73dB external atten.dwf3work")
  //const float32_t slope = 10.0;
  //const float32_t cons = -92.0;

  // prevent NAN dBm
  if(audioMaxSquaredAve > 0.0) {
    // dbm_calibration set to -22 above; gainCorrection is a value between -2 and +6 to compensate the frequency dependant pre-Amp gain
    // attenuator is 0 and could be set in a future HW revision; rfGain is initialized to 1 in the bands[] init in SDT.ino; cons=-92; slope=10

    // *** TODO: rework S-meter calibration, it's not very linear here:
    //  dbm_calibration = 24 good for S1
    //  dbm_calibration = 32 good for S9
    //  dbm_calibration = 24 good for S1
    // dbm_calibration set to 31; gainCorrection is a value between -2 and +6 to compensate the frequency dependant pre-Amp gain
    // rfGain is initialized to 1 in the bands[] init in SDT.ino; cons=-92; slope=10; rfGainAllBands is initialized to 0
    //dbm = dbm_calibration + bands[t41.ActiveBand].gainCorrection + slope * log10f_fast(audioMaxSquaredAve) + cons - (float32_t)bands[t41.ActiveBand].rfGain * 1.5 - rfGainAllBands;
    //dbm = 24.0 + bands[t41.ActiveBand].gainCorrection + 10.0 * log10f_fast(audioMaxSquaredAve) + (-92.0) - (float32_t)bands[t41.ActiveBand].rfGain * 1.5 - rfGainAllBands;
    //dbm = 32.0 + bands[t41.ActiveBand].gainCorrection + 10.0 * log10f_fast(audioMaxSquaredAve) + (-92.0) - (float32_t)bands[t41.ActiveBand].rfGain * 1.5 - rfGainAllBands;
    dbm = 38.0 + bands[t41.ActiveBand].gainCorrection + 10.0 * log10f_fast(audioMaxSquaredAve) + (-92.0) - (float32_t)bands[t41.ActiveBand].rfGain * 1.5 - rfGainAllBands;

    //if(std::isnan(dbm)) {
    //  dbm = -133.0;
    //  Serial.println("dBm is NAN");
    //}

    // adjust dBm for input RF attenuator and PSA-8+ amplifier
    dbm = dbm + ((float32_t)currentRF_InAtten) / 2.0 - 31.0;
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

  pinMode(ShutdownInPin, INPUT);
  pinMode(ShutdownOutPin, OUTPUT);
  digitalWrite(ShutdownOutPin,LOW);

  // set RF board configuration:
  //   - transmit mode relay to SSB
  //   - CW signal off
  //   - calibration relay off
  pinMode(RF_XMIT_RELAY, OUTPUT);
  digitalWrite(RF_XMIT_RELAY, XMIT_SSB);
  pinMode(RF_CW_SIGNAL, OUTPUT);
  digitalWrite(RF_CW_SIGNAL, OFF);
  pinMode(RF_CAL_RELAY, OUTPUT);
  digitalWrite(RF_CAL_RELAY, OFF);

  InitSI5351();
  AudioSetup();

  InitRFControl();

  SetRF_InAtten(currentRF_InAtten);

  InitFrontPanel();

  I2C_display();
}

void SoftResetHardware() {
  //GenTwoToneBuffer(8, 1); // 750 Hz
  //GenTwoToneBuffer(20, 2); // 1875 Hz
}

void ConfigRadioStateHardware() {
  switch(radioState) {
    case SSB_RECEIVE_STATE:
      currentRF_InAtten = RAtten[t41.ActiveBand];
      SetRF_InAtten(currentRF_InAtten);
      break;

    case SSB_TRANSMIT_STATE:
    case DATA_TRANSMIT_STATE:
      digitalWrite(RF_XMIT_RELAY, XMIT_SSB);
      //SetRF_OutAtten(powerOutSSB[t41.ActiveBand]);
      SetRF_OutAtten(currentRF_OutAtten);

      oldCenterFreq = t41.CenterFreq;
      t41.CenterFreq = t41.CenterFreq - intermediateFreq + t41.NCOFreq;
      break;

    case CW_RECEIVE_STATE:
      currentRF_InAtten = RAtten[t41.ActiveBand];
      SetRF_InAtten(currentRF_InAtten);
      break;

    case CW_TRANSMIT_STRAIGHT_STATE:
    case CW_TRANSMIT_KEYER_STATE:
      digitalWrite(RF_XMIT_RELAY, XMIT_CW);
      break;

    case DATA_RECEIVE_STATE:
      currentRF_InAtten = RAtten[t41.ActiveBand];
      SetRF_InAtten(currentRF_InAtten);
      break;

    default:
      break;
  }
}

void HardwareLoopStart() {
  // check for signal to begin shutdown and perform shutdown routine if requested
  if(digitalRead(ShutdownInPin) == HIGH) {
    ShutDownRoutine();
  }
}

//------------
// Utility.cpp

void GenTwoToneBuffer(int numCycles, int tone) {
  // side tone freq = numCycles * 24000 / 256;
  float theta;

  for(int i = 0; i < 256; i++) {
    // theta = i * 2 * PI * freq / 24000
    theta = i * 2.0 * PI * numCycles / 256;
    switch(tone) {
      case 1:
        //cosBuffer4[i] = cos(theta);
        sinBuffer4[i] = sin(theta);
        break;

      case 2:
        //cosBuffer5[i] = cos(theta);
        sinBuffer5[i] = sin(theta);
        break;

      default:
        break;
    }
  }
}

const byte ShutdownInPin = 0;
const byte ShutdownOutPin = 1;

//int t_press = 0;
void ShutDownRoutine() {
// my delay code
/*
  static unsigned long t_press = 0;

  // perform shutdown processing first time through
  if(t_press == 0) {
    // perform any desired shutdown processing here

    // set shutdown timer
    t_press = millis();
  }

  // shutdown after 100ms
  // we'll continue processing main loop until then
  if(millis() - t_press > 100) {
    // pull the ATTiny shutdown pin high indicating that we have
    // finished shutdown and it's safe to power off the T41
    digitalWrite(ShutdownOutPin, 1);
    t_press = millis();
  }
*/

// v66-9 code
/*
  // Do shutdown stuff. Nothing here yet
  // Tell the ATTiny that we have finished shutdown and it's safe to power off

  if (t_press - (int)millis() > 100 ){
    digitalWrite(ShutdownOutPin, 0);
  } else {
    digitalWrite(ShutdownOutPin, 1);
    t_press = millis();
  }
*/

  // Do shutdown stuff. Nothing here yet

  // simplified w/o delay
  digitalWrite(ShutdownOutPin, 1);
}

/*****
  Purpose: Display the status of the I2C peripherals on start-up.*****/
void I2C_display() {
  char tmpbuf[80];
  uint32_t xpos, ypos = YPIXELS / 4;
  bool i2cFailure = false;

  tft.fillWindow(RA8875_BLACK);

  tft.setFontScale(1);
  tft.setTextColor(DARKGREY);
  tft.setCursor(XPIXELS / 3 - 100, YPIXELS / 10);
  tft.print("I2C Status Report");

  tft.setFontScale(0);

  xpos = 5 * tft.getFontWidth();
  tft.setCursor(xpos, ypos);
  if(bit_results.FRONT_PANEL_I2C_1_present) {
    tft.setTextColor(RA8875_GREEN);
  } else {
    tft.setTextColor(RA8875_RED);
    tft.print("No ");
    i2cFailure = true;
  }
  sprintf(tmpbuf, "Front panel MCP23017 at I2C 0x%02X", V12_PANEL_MCP23017_ADDR_1);
  tft.print(tmpbuf);
  ypos += 30;

  tft.setCursor(xpos, ypos);
  if(bit_results.FRONT_PANEL_I2C_2_present) {
    tft.setTextColor(RA8875_GREEN);
  } else {
    tft.setTextColor(RA8875_RED);
    tft.print("No ");
    i2cFailure = true;
  }
  sprintf(tmpbuf, "Front panel MCP23017 at I2C 0x%02X", V12_PANEL_MCP23017_ADDR_2);
  tft.print(tmpbuf);
  ypos += 30;


  tft.setCursor(xpos, ypos);
  if(bit_results.BPF_I2C_present) {
    tft.setTextColor(RA8875_GREEN);
  } else {
    tft.setTextColor(RA8875_RED);
    tft.print("No ");
    i2cFailure = true;
  }
  sprintf(tmpbuf, "BPF MCP23017 at I2C 0x%02X", BPF_MCP23017_ADDR);
  tft.print(tmpbuf);
  ypos += 30;

  tft.setCursor(xpos, ypos);
  if(bit_results.RF_I2C_present) {
    tft.setTextColor(RA8875_GREEN);
  } else {
    tft.setTextColor(RA8875_RED);
    tft.print("No ");
    i2cFailure = true;
  }
  sprintf(tmpbuf, "RF MCP23017 at I2C 0x%02X", RF_MCP23017_ADDR);
  tft.print(tmpbuf);
  ypos += 30;

  tft.setCursor(xpos, ypos);
  if(bit_results.V12_LPF_I2C_present) {
    tft.setTextColor(RA8875_GREEN);
  } else {
    tft.setTextColor(RA8875_RED);
    tft.print("No ");
    i2cFailure = true;
  }
  sprintf(tmpbuf, "LPF MCP23017 at I2C 0x%02X", V12_LPF_MCP23017_ADDR);
  tft.print(tmpbuf);
  ypos += 30;

  tft.setCursor(xpos, ypos);
  if(bit_results.RF_Si5351_present) {
    tft.setTextColor(RA8875_GREEN);
  } else {
    tft.setTextColor(RA8875_RED);
    tft.print("No ");
    i2cFailure = true;
  }
  sprintf(tmpbuf, "RF SI5351 at I2C 0x%02X", SI5351_BUS_BASE_ADDR);
  tft.print(tmpbuf);
  ypos += 30;

#ifdef V12_LPF_SWR_AD7991
  tft.setCursor(xpos, ypos);
  if(bit_results.V12_LPF_AD7991_present) {
    tft.setTextColor(RA8875_GREEN);
    sprintf(tmpbuf, "LPF AD7991 at I2C 0x%02X", AD7991_I2C_ADDR);
  } else {
    tft.setTextColor(RA8875_RED);
    sprintf(tmpbuf, "No LPF AD7991 I2C at 0x%02X or 0x%02X", AD7991_I2C_ADDR1, AD7991_I2C_ADDR2);
    i2cFailure = true;
  }
  tft.print(tmpbuf);
  ypos += 30;
#endif  // V12_LPF_SWR_AD7991

  if(i2cFailure) {
    delay(I2C_DELAY_LONG);
  } else {
    delay(I2C_DELAY_SHORT);
  }

  tft.fillWindow(RA8875_BLACK);
}
