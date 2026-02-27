
#include "SDT.h"

#include "Beacon.h"
#include "Bearing.h"
#include "Button.h"
#include "ButtonProc.h"
#include "src\Calibrate.h"
#include "CWProcessing.h"
#include "CW_Excite.h"
#include "Display.h"
#include "DSP_Fn.h"
#include "EEPROM.h"
#include "Encoders.h"
#include "Exciter.h"
#include "Filter.h"
#include "ft8.h"
#include "InfoBox.h"
#include "Menu.h"
#include "MenuProc.h"
#include "mouse.h"
#include "Tune.h"
#include "Utility.h"

//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

#define MAX_WPM                  60

int calibrateItem = -1;

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
      GetMenuValue(5, MAX_WPM, &currentWPM, 1, "WPM:", 200, NULL, NULL, &SetWPMFollowup);
      break;

    case 1:          // Type of key:
      SetKeyType();  // Straight key or keyer? Stored in EEPROMData.keyType; no heap/stack variable
      SetKeyPowerUp();
      UpdateInfoBoxItem(IB_ITEM_KEY);
      break;

    case 2:              // CW Filter BW
      SelectCWFilter();  // in CWProcessing
      break;

    case 3:            // Flip paddles
      DoPaddleFlip();  // Stored in EEPROM; variables paddleDit and paddleDah
      break;

    case 4:  // Sidetone volume
      //SetSideToneVolume();
      // GetMenuValue(minValue, maxValue, startValue, increment, prompt, valueOffset)
      GetMenuValue(0, 100, &sidetoneVolume, 1, "Volume:", 200, &SetSideToneVolumeSetup, &SetSideToneVolumeValue, &SetSideToneVolumeFollowup);
  break;

    case 5:                // Transmit relay hold delay
      //SetTransmitDelay();
      GetMenuValue(0, 9750, (int*)&cwTransmitDelay, 250, "Delay:", 150, NULL, NULL, &SetTransmitDelayFollowup);
      break;

    default:  // Cancel
      break;
  }
}

// *** TODO: T41EEE does this for each band ***
FLASHMEM void RFPowerFollowup() {
  if(radioMode == CW_MODE) {                                                                                                                                      //AFP 10-13-22
    powerOutCW[currentBand] = (-.0133 * transmitPowerLevel * transmitPowerLevel + .7884 * transmitPowerLevel + 4.5146) * CWPowerCalibrationFactor[currentBand];  //  afp 10-21-22

    EEPROMData.powerOutCW[currentBand] = powerOutCW[currentBand];
  } else {
    if(radioMode == SSB_MODE) {
      powerOutSSB[currentBand] = (-.0133 * transmitPowerLevel * transmitPowerLevel + .7884 * transmitPowerLevel + 4.5146) * SSBPowerCalibrationFactor[currentBand];  // afp 10-21-22
      EEPROMData.powerOutSSB[currentBand] = powerOutSSB[currentBand];                                                                                                //AFP 10-21-22
    }
  }
  EEPROMData.transmitPowerLevel = transmitPowerLevel;
  EEPROMWrite();
  ShowCurrentPowerSetting();
}

FLASHMEM void RFGainFollowup() {
  EEPROMData.rfGainAllBands = rfGainAllBands;
  EEPROMWrite();
  UpdateInfoBoxItem(IB_ITEM_RFGAIN);
}

/*****
  Purpose: Used to change the currently active VFO
*****/
FLASHMEM void VFOSelect(int32_t index) {
  if(radioMode == DATA_MODE) {
    // restore old demodulation mode before we change bands
    bands[currentBand].demod = priorDemodMode;
  }

  splitVFO = false;
  NCOFreq = 0L;

  switch(index) {
    case VFO_A:
      centerFreq = TxRxFreq = currentFreqA;
      activeVFO = VFO_A;
      currentBand = currentBandA;
      //tft.fillRect(FILTER_PARAMETERS_X + 180, FILTER_PARAMETERS_Y, 150, 20, RA8875_BLACK);  // Erase split message
      break;

    case VFO_B:
      centerFreq = TxRxFreq = currentFreqB;
      activeVFO = VFO_B;
      currentBand = currentBandB;
      //tft.fillRect(FILTER_PARAMETERS_X + 180, FILTER_PARAMETERS_Y, 150, 20, RA8875_BLACK);  // Erase split message
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
  if(radioMode == DATA_MODE) {
    priorDemodMode = bands[currentBand].demod; // save demod mode for restoration later

    switch(bands[currentBand].demod) {
      case DEMOD_PSK31_WAV:
      case DEMOD_PSK31:
        bands[currentBand].demod = DEMOD_PSK31;
        break;

      case DEMOD_FT8_DECODE:
      case DEMOD_FT8_WAV:
        bands[currentBand].demod = DEMOD_FT8_DECODE;
        syncFlag = false;
        ft8State = 1;
        UpdateInfoBoxItem(IB_ITEM_FT8);
        break;
    }
  }
  */

  bands[currentBand].freq = TxRxFreq;
  SetBand();                            // SetBand updates the display
  SetBandRelay(HIGH);                   // Required when switching VFOs

  EEPROMData.activeVFO = activeVFO;
  EEPROMWrite();

  if(radioMode == CW_MODE) {
    UpdateCWFilter();
  }
}

FLASHMEM void VFOSelect() {
  VFOSelect(secondaryMenuIndex);
}

/*****
  Purpose: Allow user to set current EEPROM values or restore default settings
*****/
FLASHMEM void EEPROMOptions() {
  //  const char *EEPROMOpts[] = { "Save Current", "Set Defaults", "Get Favorite", "Set Favorite",
  //                               "Copy EEPROM-->SD", "Copy SD-->EEPROM", "SD EEPROM Dump", "Cancel" };
  switch(secondaryMenuIndex) {
    case 0:  // Save current values
      EEPROMWrite();
      break;

    case 1:
      EEPROMSaveDefaults2();  // Restore defaults
      break;

    case 2:
      GetFavoriteFrequency();  // Get a stored frequency and store in active VFO
      break;

    case 3:
      SetFavoriteFrequency();  // Set favorites
      break;

    case 4:
      CopyEEPROMToSD();  // Save current EEPROM value to SD
      break;

    case 5:
      CopySDToEEPROM();  // Copy from SD to EEPROM
      EEPROMRead();
      tft.writeTo(L2);   // This is specifically to clear the bandwidth indicator bar.  KF5N August 7, 2023
      tft.clearMemory();
      tft.writeTo(L1);
      RedrawDisplayScreen();  // Assume there are lots of changes and do a heavy-duty refresh.  KF5N August 7, 2023
      break;

    case 6:
      SDEEPROMDump();  // Show SD data
      break;

    default:
      break;
  }
}

/*****
  Purpose: Present the bands available and return the selection
*****/
FLASHMEM void AGCOptions() {
  // const char *AGCChoices[] = { "Off", "Long", "Slow", "Medium", "Fast", "Cancel" }; // G0ORX (Added Long) September 5, 2023

  AGCMode = secondaryMenuIndex;
  AGCLoadValues();

  EEPROMData.AGCMode = AGCMode; // Store in EEPROM and...
  EEPROMWrite();  // ...save it
  UpdateInfoBoxItem(IB_ITEM_AGC);
}

/*****
  Purpose: Show the list of scales for the spectrum divisions
*****/
FLASHMEM void SpectrumOptions() {
  //const char *spectrumChoices[] = { "20 dB/unit", "10 dB/unit", "5 dB/unit", "2 dB/unit", "1 dB/unit", "Cancel" };
  int spectrumSet = EEPROMData.currentScale;

  spectrumSet = secondaryMenuIndex;
  //if(strcmp(spectrumChoices[spectrumSet], "Cancel") == 0) {
  if(spectrumSet == 5) {
    return;
  }
  currentScale = spectrumSet;  // Yep...
  EEPROMData.currentScale = currentScale;
  EEPROMWrite();
  ShowSpectrumdBScale();
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
      EEPROMWrite();
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
      EEPROMWrite();
      RedrawDisplayScreen();
      break;
    case 3:
      break;
  }
}

FLASHMEM void MicGainFollowup() {
  EEPROMData.currentMicGain = currentMicGain;
  EEPROMWrite();
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
  EEPROMData.currentMicThreshold = currentMicThreshold;
  EEPROMWrite();
  UpdateInfoBoxItem(IB_ITEM_COMPRESS);
}

/*
FLASHMEM void SetCompressionRatioFollowup() {
  //currentMicCompRatio += ((float) menuEncoderMove * .1);

  EEPROMData.currentMicCompRatio = currentMicCompRatio;
  EEPROMWrite();
}

FLASHMEM void SetCompressionAttackFollowup() {
  //currentMicAttack += ((float) menuEncoderMove * 0.1);
  //else if(currentMicAttack < .1)
  //  currentMicAttack = .1;

  EEPROMData.currentMicAttack = currentMicAttack;
  EEPROMWrite();
}

FLASHMEM void SetCompressionReleaseFollowup() {
  //currentMicRelease += ((float) menuEncoderMove * 0.1);
  //else if(currentMicRelease < 0.1)                 // 100% max
  //  currentMicRelease = 0.1;

  EEPROMData.currentMicCompRatio = currentMicCompRatio;
  EEPROMWrite();
}
*/

/*****
  Purpose: Turn mic compression on and set the level
*****/
FLASHMEM void MicOptions() {
  //  const char *micChoices[] = { "On", "Off", "Set Threshold", "Set Comp_Ratio", "Set Attack", "Set Decay", "Cancel" };
  switch(secondaryMenuIndex) {
    case 0:                // On
      compressorFlag = 1;
      UpdateInfoBoxItem(IB_ITEM_COMPRESS);
      break;

    case 1:  // Off
      compressorFlag = 0;
      UpdateInfoBoxItem(IB_ITEM_COMPRESS);
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
  Purpose: To process the graphics for the 14 chan equalizar otpion

  Parameter list:
    int array[]         the peoper array to fill in
    char *title             the equalizer being set
  Return value
    void
*****/
FLASHMEM void ProcessEqualizerChoices(int EQType, char *title) {
  for(int i = 0; i < EQUALIZER_CELL_COUNT; i++) {
  }
  const char *eqFreq[] = { " 200", " 250", " 315", " 400", " 500", " 630", " 800",
                           "1000", "1250", "1600", "2000", "2500", "3150", "4000" };
  int yLevel[EQUALIZER_CELL_COUNT];

  int columnIndex;
  int iFreq;
  int newValue;
  int xOrigin = 50;
  int xOffset;
  int yOrigin = 50;
  int wide = 700;
  int high = 300;
  int barWidth = 46;
  int barTopY;
  int barBottomY;
  int val;

  for(iFreq = 0; iFreq < EQUALIZER_CELL_COUNT; iFreq++) {
    if(EQType == 0) {
      yLevel[iFreq] = EEPROMData.equalizerRec[iFreq];
    } else {
      if(EQType == 1) {
        yLevel[iFreq] = EEPROMData.equalizerXmt[iFreq];
      }
    }
  }
  tft.writeTo(L2);
  tft.clearMemory();
  tft.writeTo(L1);
  tft.fillWindow(RA8875_BLACK);

  tft.fillRect(xOrigin - 50, yOrigin - 25, wide + 50, high + 50, RA8875_BLACK);  // Clear data area
  tft.setTextColor(RA8875_GREEN);
  tft.setFontScale((enum RA8875tsize)1);
  tft.setCursor(200, 0);
  tft.print(title);

  tft.drawRect(xOrigin - 4, yOrigin, wide + 4, high, RA8875_BLUE);
  tft.drawFastHLine(xOrigin - 4, yOrigin + (high / 2), wide + 4, RA8875_RED);  // Print center zero line center
  tft.setFontScale((enum RA8875tsize)0);

  tft.setTextColor(RA8875_WHITE);
  tft.setCursor(xOrigin - 4 - tft.getFontWidth() * 3, yOrigin + tft.getFontHeight());
  tft.print("+12");
  tft.setCursor(xOrigin - 4 - tft.getFontWidth() * 3, yOrigin + (high / 2) - tft.getFontHeight());
  tft.print(" 0");
  tft.setCursor(xOrigin - 4 - tft.getFontWidth() * 3, yOrigin + high - tft.getFontHeight() * 2);
  tft.print("-12");

  barTopY = yOrigin + (high / 2);                // 50 + (300 / 2) = 200
  barBottomY = barTopY + DEFAULT_EQUALIZER_BAR;  // Default 200 + 100

  for(iFreq = 0; iFreq < EQUALIZER_CELL_COUNT; iFreq++) {
    tft.fillRect(xOrigin + (barWidth + 4) * iFreq, barTopY - (yLevel[iFreq] - DEFAULT_EQUALIZER_BAR), barWidth, yLevel[iFreq], RA8875_CYAN);
    tft.setCursor(xOrigin + (barWidth + 4) * iFreq, yOrigin + high - tft.getFontHeight() * 2);
    tft.print(eqFreq[iFreq]);
    tft.setCursor(xOrigin + (barWidth + 4) * iFreq + tft.getFontWidth() * 1.5, yOrigin + high + tft.getFontHeight() * 2);
    tft.print(yLevel[iFreq]);
  }

  columnIndex = 0;  // Get ready to set values for columns
  newValue = 0;
  while(columnIndex < EQUALIZER_CELL_COUNT) {
    xOffset = xOrigin + (barWidth + 4) * columnIndex;   // Just do the math once
    tft.fillRect(xOffset,                               // Indent to proper bar...
                 barBottomY - yLevel[columnIndex] - 1,  // Start at red line
                 barWidth,                              // Set bar width
                 newValue + 1,                          // Erase old bar
                 RA8875_BLACK);

    tft.fillRect(xOffset,                           // Indent to proper bar...
                 barBottomY - yLevel[columnIndex],  // Start at red line
                 barWidth,                          // Set bar width
                 yLevel[columnIndex],               // Draw new bar
                 RA8875_MAGENTA);
    while(true) {
      newValue = yLevel[columnIndex];  // Get current value
      if(menuEncoderMove != 0) {

        tft.fillRect(xOffset,                    // Indent to proper bar...
                     barBottomY - newValue - 1,  // Start at red line
                     barWidth,                   // Set bar width
                     newValue + 1,               // Erase old bar
                     RA8875_BLACK);
        newValue += (PIXELS_PER_EQUALIZER_DELTA * menuEncoderMove);  // Find new bar height. OK since menuEncoderMove equals 1 or -1
        tft.fillRect(xOffset,                                          // Indent to proper bar...
                     barBottomY - newValue,                            // Start at red line
                     barWidth,                                         // Set bar width
                     newValue,                                         // Draw new bar
                     RA8875_MAGENTA);
        yLevel[columnIndex] = newValue;

        tft.fillRect(xOffset + tft.getFontWidth() * 1.5 - 1, yOrigin + high + tft.getFontHeight() * 2,  // Update bottom number
                     barWidth, CHAR_HEIGHT, RA8875_BLACK);
        tft.setCursor(xOffset + tft.getFontWidth() * 1.5, yOrigin + high + tft.getFontHeight() * 2);
        tft.print(yLevel[columnIndex]);
        if(newValue < DEFAULT_EQUALIZER_BAR) {  // Repaint red center line if erased
          tft.drawFastHLine(xOrigin - 4, yOrigin + (high / 2), wide + 4, RA8875_RED);
          ;  // Clear hole in display center
        }
      }
      menuEncoderMove = 0;
      delay(200L);

      val = ReadSelectedPushButton();  // Read the ladder value

      if(val != BOGUS_PIN_READ) {
        val = ProcessButtonPress(val);  // Use ladder value to get menu choice
        delay(100L);

        tft.fillRect(xOffset,                // Indent to proper bar...
                     barBottomY - newValue,  // Start at red line
                     barWidth,               // Set bar width
                     newValue,               // Draw new bar
                     RA8875_GREEN);

        if(EQType == 0) {
          equalizerRec[columnIndex] = newValue;
          EEPROMData.equalizerRec[columnIndex] = equalizerRec[columnIndex];
        } else {
          if(EQType == 1) {
            equalizerXmt[columnIndex] = newValue;
            EEPROMData.equalizerXmt[columnIndex] = equalizerXmt[columnIndex];
          }
        }

        menuEncoderMove = 0;
        columnIndex++;
        break;
      }
    }
  }

  EEPROMWrite();
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
      beaconFlag = true;
      break;
    case 1: // off
      BeaconExit();
      beaconFlag = false;
      break;
    case 2:
      break;
  }
}
