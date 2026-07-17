
#include "SDT.h"

#include "Button.h"
#include "ButtonProc.h"
#include "calibrate.h"
#include "CWProcessing.h"
#include "CW_Excite.h"
#include "Display.h"
#include "DSP_Fn.h"
#include "Encoders.h"
#include "Exciter.h"
#include "Filter.h"
#include "ft8.h"
#include "Menu.h"
#include "MenuProc.h"
#include "mouse.h"
#include "Tune.h"
#include "Utility.h"

//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

#define MAX_WPM                  60

int currentMicThreshold = -10;
float currentMicCompRatio = 5.0;
float currentMicAttack = 0.1;
float currentMicRelease = 2.0;
int currentMicGain = -10;

//-------------------------------------------------------------------------------------------------------------
// Forwards
//-------------------------------------------------------------------------------------------------------------

void ProcessEqualizerChoices(int EQType, char *title);

//-------------------------------------------------------------------------------------------------------------
// Code
//-------------------------------------------------------------------------------------------------------------

/*****
  Purpose: Present the CW options available and return the selection
*****/
FLASHMEM void CWOptions() {
  // const char *cwChoices[] = { "WPM", "Key Type", "CW Filter", "Paddle Flip", "Sidetone Volume", "Transmit Delay", "Cancel" };  // AFP 10-18-22

  //Serial.println(secondaryMenuIndex);
  //Serial.println(menuBarSelected);
  switch(secondaryMenuIndex) {
    case 0:  // WPM
      //SetWPM();
      // GetMenuValue(minValue, maxValue, startValue, increment, prompt, valueOffset)
      GetMenuValue(5, MAX_WPM, (int*)&t41.CurrentWPM.value, 1, "WPM:", 200, NULL, NULL, &SetWPMFollowup);
      break;

    case 1:          // Type of key:
      SetKeyType();  // Straight key or keyer?
      SetKeyPowerUp();
      UpdateInfoBoxItem(T41_ITEM_KEY);
      break;

    case 2:              // CW Filter BW
      SelectCWFilter();  // in CWProcessing
      break;

    case 3:            // Flip paddles
      DoPaddleFlip();
      break;

    case 4:  // Sidetone volume
      //SetSideToneVolume();
      // GetMenuValue(minValue, maxValue, startValue, increment, prompt, valueOffset)
      GetMenuValue(0, 100, (int*)&t41.SidetoneVolume.value, 1, "Volume:", 200, &SetSideToneVolumeSetup, &SetSideToneVolumeValue, &SetSideToneVolumeFollowup);
      break;

    case 5:                // Transmit relay hold delay
      //SetTransmitDelay();
      GetMenuValue(0, 9750, (int*)&t41.CWTransmitDelay.value, 250, "Delay:", 150, NULL, NULL, &SetTransmitDelayFollowup);
      break;

    default:  // Cancel
      break;
  }
}

// *** TODO: T41EEE does this for each band ***
FLASHMEM void RFPowerFollowup() {
  if(t41.RadioMode == CW_MODE) {                                                                                                                                      //AFP 10-13-22
    powerOutCW[t41.ActiveBand] = (-.0133 * t41.TxPower * t41.TxPower + .7884 * t41.TxPower + 4.5146) * CWPowerCalibrationFactor[t41.ActiveBand];  //  afp 10-21-22

  } else {
    if(t41.RadioMode == SSB_MODE) {
      powerOutSSB[t41.ActiveBand] = (-.0133 * t41.TxPower * t41.TxPower + .7884 * t41.TxPower + 4.5146) * SSBPowerCalibrationFactor[t41.ActiveBand];  // afp 10-21-22
    }
  }
  ShowCurrentPowerSetting();
}

FLASHMEM void RFGainFollowup() {
}

/*****
  Change the currently active VFO
*****/
FLASHMEM void VFOSelect(int32_t index) {
  splitVFO = false;

  switch(index) {
    case VFO_A:
    case VFO_B:
      t41.SwapActiveVFO();
      break;

    case VFO_SPLIT:
      DoSplitVFO();
      break;

    default:  // Cancel
      return;
      break;
  }

  // *** TODO: this needs reworked ***
  /*
  if(t41.RadioMode == DATA_MODE) {
    priorDemodMode = t41.DemodMode; // save demod mode for restoration later

    switch(t41.DemodMode) {
      case DEMOD_PSK31_WAV:
      case DEMOD_PSK31:
        t41.DemodMode = DEMOD_PSK31;
        break;

      case DEMOD_FT8_INTERNAL:
      case DEMOD_FT8_WAV:
        t41.DemodMode = DEMOD_FT8_INTERNAL;
        ft8SyncState = 0;
        UpdateInfoBoxItem(T41_ITEM_FT8);
        break;
    }
  }
  */

  bands[t41.ActiveBand].freq = t41.CenterFreq;

  SetupBandFreq(t41.CenterFreq);
  // *** TODO: this seems oddly placed and specific to only v11, investigate ***
  //SetBandRelay(HIGH); // Required when switching VFOs

  if(t41.RadioMode == CW_MODE) {
    DrawCWFilter();
  }
}

FLASHMEM void VFOSelect() {
  VFOSelect(secondaryMenuIndex);
}

/*****
  set agc to selected option
*****/
FLASHMEM void AGCOptions() {
  // const char *AGCChoices[] = { "Off", "Long", "Slow", "Medium", "Fast", "Cancel" };

  t41.AGCMode = secondaryMenuIndex;
}

/*****
  Purpose: Show the list of scales for the spectrum divisions
*****/
FLASHMEM void SpectrumOptions() {
  //const char *spectrumChoices[] = { "20 dB/unit", "10 dB/unit", "5 dB/unit", "2 dB/unit", "1 dB/unit", "Cancel" };
  int spectrumSet = secondaryMenuIndex;

  //if(strcmp(spectrumChoices[spectrumSet], "Cancel") == 0) {
  if(spectrumSet == 5) {
    return;
  }
  t41.FreqSpecScale = spectrumSet;
}

/*****
  Purpose: Receive EQ set
*****/
FLASHMEM void EqualizerRecOptions() {
  //  const char *RecEQChoices[] = { "On", "Off", "EQSet", "Cancel" };
 switch(secondaryMenuIndex) {
     case 0:
      receiveEQFlag = ON;
      break;
    case 1:
      receiveEQFlag = OFF;
      break;
    case 2:
      for(int iFreq = 0; iFreq < EQUALIZER_CELL_COUNT; iFreq++) {
      }
      ProcessEqualizerChoices(0, (char *)"Receive Equalizer");
      RedrawDisplayScreen();
      break;
    case 3:
      break;
  }
}

/*****
  Purpose: Xmit EQ options
*****/
FLASHMEM void EqualizerXmtOptions() {
  //  const char *XmtEQChoices[] = { "On", "Off", "EQSet", "Cancel" };
 switch(secondaryMenuIndex) {
    case 0:
      xmitEQFlag = ON;
      break;
    case 1:
      xmitEQFlag = OFF;
      break;
    case 2:
      ProcessEqualizerChoices(1, (char *)"Transmit Equalizer");
      RedrawDisplayScreen();
      break;
    case 3:
      break;
  }
}

FLASHMEM void MicGainFollowup() {
}

/*****
  Purpose: Set mic gain level
*****/
FLASHMEM void MicGainSet() {
  //  const char *micGainChoices[] = { "Set Mic Gain", "Cancel" };
  switch(secondaryMenuIndex) {
    case 0:
      // GetMenuValue(minValue, maxValue, startValue, increment, prompt, valueOffset)
      GetMenuValue(-40, 30, &currentMicGain, 1, "Gain:", 200, NULL, NULL, &MicGainFollowup);
      break;

    case 1:
      break;
  }
}

FLASHMEM void SetCompressionLevelFollowup() {
  UpdateInfoBoxItem(T41_ITEM_COMPRESS);
}

/*
FLASHMEM void SetCompressionRatioFollowup() {
  //currentMicCompRatio += ((float) menuEncoderMove * .1);
}

FLASHMEM void SetCompressionAttackFollowup() {
  //currentMicAttack += ((float) menuEncoderMove * 0.1);
  //else if(currentMicAttack < .1)
  //  currentMicAttack = .1;
}

FLASHMEM void SetCompressionReleaseFollowup() {
  //currentMicRelease += ((float) menuEncoderMove * 0.1);
  //else if(currentMicRelease < 0.1)                 // 100% max
  //  currentMicRelease = 0.1;
}
*/

/*****
  Purpose: Turn mic compression on and set the level
*****/
FLASHMEM void MicOptions() {
  //  const char *micChoices[] = { "On", "Off", "Set Threshold", "Set Comp_Ratio", "Set Attack", "Set Decay", "Cancel" };
  switch(secondaryMenuIndex) {
    case 0:                // On
      t41.Compressor = 1;
      break;

    case 1:  // Off
      t41.Compressor = 0;
      break;

    case 2:
      //SetCompressionLevel();
      GetMenuValue(-60, 00, &currentMicThreshold, 1, "Compression:", 200, NULL, NULL, &SetCompressionLevelFollowup);
      break;

    /* *** TODO: these aren't used currently ***
    case 3:
      //SetCompressionRatio();
      // *** TODO: FIX: original had increment at 0.1 ***
      //GetMenuValue(1, 10, &currentMicCompRatio, 1, "Ratio:", 200, NULL, NULL, &SetCompressionRatioFollowup);
      break;

    case 4:
      //SetCompressionAttack();
      // *** TODO: FIX: original had min and increment at 0.1 ***
      //GetMenuValue(1, 10, &currentMicAttack, 1, "Ratio:", 200, NULL, NULL, &SetCompressionAttackFollowup);
      break;

    case 5:
      //SetCompressionRelease();
      // *** TODO: FIX: original had increment at 0.1 ***
      //GetMenuValue(1, 10, &currentMicRelease, 1, "Ratio:", 200, NULL, NULL, &SetCompressionReleaseFollowup);
      break;

    case 6:
      break;
    */
    default:  // Cancelled choice
      break;
  }
  secondaryMenuIndex = -1;
}

/*****
  Present the calibration options available and return the selection
*****/
FLASHMEM void CalibrationOptions() {
  // Calibrate { "Freq", "RX IQ", "TX IQ", "Two Tone", "CW Pwr", "SSB Pwr", "Exit", "Cancel" },

  if(secondaryMenuIndex == 7) {
    CalibrationExit();
  } else {
    CalibrationInit(secondaryMenuIndex);
  }
}

/*****
  Purpose: Process bearing map options
*****/
FLASHMEM void BearingOptions() {
  //  const char *BearingChoices[] = { "Show Map", "Set Prefix", "Cancel" };
  switch(secondaryMenuIndex) {
     case 0:
      ButtonBearing();
      break;
    case 1:
      BearingMaps();
      break;
    case 2:
      break;
  }
}

/*****
  Purpose: Turn beacon monitor on or off
*****/
FLASHMEM void BeaconOptions() {
  //  const char *BeaconChoices[] = { "On", "Off", "Cancel" };
  switch(secondaryMenuIndex) {
     case 0: // on
      BeaconInit();
      break;
    case 1: // off
      BeaconExit();
      break;
    case 2:
      break;
  }
}
