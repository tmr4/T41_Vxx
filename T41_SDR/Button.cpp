#include "SDT.h"

#include "Beacon.h"
#include "Button.h"
#include "ButtonProc.h"
#include "src\Calibrate.h"
#include "Display.h"
#include "EEPROM.h"
#include "src\FrontPanel.h"
#include "ft8.h"
#include "InfoBox.h"
#include "Menu.h"
#include "MenuProc.h"
#include "Process.h"
#include "psk31.h"
#include "Tune.h"
#include "Utility.h"

#include "debug.h"

//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

//-------------------------------------------------------------------------------------------------------------
// Forwards
//-------------------------------------------------------------------------------------------------------------

//-------------------------------------------------------------------------------------------------------------
// Code
//-------------------------------------------------------------------------------------------------------------

/*****
  Purpose: Function is designed to route program control to the proper execution point in response to
           a button press.  To avoid duplication, display updates are generally handled in the routed routine.

  Parameter list:
    int vsl               the value from analogRead in loop()
*****/
FLASHMEM void ExecuteButtonPress(int val) {
#ifdef DEBUG_SW
  Serial.print("ExecuteButtonPress TOP: val = ");
  Serial.println(val);
#endif
  switch(val) {
    case MENU_OPTION_SELECT:  // 0

      if(USE_FULL_MENU) {
        if(val == MENU_OPTION_SELECT && menuStatus == NO_MENUS_ACTIVE) {  // Pressed Select with no primary/secondary menu selected
#ifdef DEBUG_SW
  //NoActiveMenu();
  Serial.print("NAM #0: val = ");
  Serial.println(val);
#endif
          return;
        } else {
          menuStatus = PRIMARY_MENU_ACTIVE;
        }

        if(menuStatus == PRIMARY_MENU_ACTIVE) {  // Doing primary menu
          ErasePrimaryMenu();
          functionPtr[mainMenuIndex]();  // These are processed in MenuProc.cpp
          menuStatus = SECONDARY_MENU_ACTIVE;
          secondaryMenuIndex = -1;  // Reset secondary menu
        } else {
          if(menuStatus == SECONDARY_MENU_ACTIVE) {  // Doing primary menu
            menuStatus = PRIMARY_MENU_ACTIVE;
            mainMenuIndex = 0;
          }
        }

        EraseMenus();
      } else {
        MenuBarSelect();
      }
      break;

    case MAIN_MENU_UP:  // 1
      if(USE_FULL_MENU) {
        displayState = DISPLAY_FULL_MENU;
        DrawMenuDisplay();                    // Draw selection box and primary menu
        SetPrimaryMenuIndex();                // Scroll through primary indexes and select one
        if(mainMenuIndex < TOP_MENU_COUNT - 1) {
          SetSecondaryMenuIndex();              // Use the primary index selection to redraw the secondary menu and set its index
          functionPtr[mainMenuIndex]();
        }

        displayState = DISPLAY_T41;
        tft.fillRect(1, SPECTRUM_TOP_Y + 1, 513, 379, RA8875_BLACK);          // Erase Menu box
        DrawSpectrumFrame();
        DrawBandwidthBar();
        ShowBandwidthBarValues();
        ShowSpectrumFreqValues();
      } else {
        ShowMenuBar(0, 1);
      }
      break;

    case BAND_UP:  // 2
      ChangeBand(1);
      break;

    case ZOOM:  // 3
      SetZoom(spectrumZoom+1);
      break;

    case MAIN_MENU_DN:  // 4
      if(USE_FULL_MENU) {
        displayState = DISPLAY_FULL_MENU;
        DrawMenuDisplay();                    // Draw selection box and primary menu
        SetPrimaryMenuIndex();                // Scroll through primary indexes and select one
        if(mainMenuIndex < TOP_MENU_COUNT - 1) {
          SetSecondaryMenuIndex();              // Use the primary index selection to redraw the secondary menu and set its index
          functionPtr[mainMenuIndex]();
        }

        displayState = DISPLAY_T41;
        tft.fillRect(1, SPECTRUM_TOP_Y + 1, 513, 379, RA8875_BLACK);          // Erase Menu box
        DrawSpectrumFrame();
        DrawBandwidthBar();
        ShowBandwidthBarValues();
        ShowSpectrumFreqValues();
      } else {
        ShowMenuBar(TOP_MENU_COUNT - 2, -1);
      }
      break;

    case BAND_DN:  // 5
      ChangeBand(-1);
      break;

    case FILTER:  // 6
      ButtonFilter();
      break;

    case DEMODULATION:  // 7
      // change to the next demod mode
      ChangeDemodMode(bands[currentBand].demod + 1);
      break;

    case SET_MODE:  // 8
      // change to the next mode: SSB -> CW -> DATA -> SSB
      ChangeMode(radioMode + 1);
      break;

    case NOISE_REDUCTION:  // 9
      ButtonNR();
      break;

    case NOTCH_FILTER:  // 10
      ButtonNotchFilter();
      UpdateInfoBoxItem(IB_ITEM_NOTCH);
      break;

    case NOISE_FLOOR:  // 11
      ToggleLiveNoiseFloorFlag();
      break;

    case FINE_TUNE_INCREMENT:  // 12
      ChangeFtIncrement(1);
      break;

    case DECODER_TOGGLE:  // 13
      decoderFlag = !decoderFlag;
      UpdateInfoBoxItem(IB_ITEM_DECODER);

      if(radioMode == CW_MODE) {
        if(decoderFlag == ON) {
          // reduce waterfall height if we're decoding CW
          tft.fillRect(WATERFALL_L, YPIXELS - 35, WATERFALL_W, CHAR_HEIGHT + 3, RA8875_BLACK);  // Erase waterfall in decode area
          tft.writeTo(L2); // it's on layer 2 as well
          tft.fillRect(WATERFALL_L, YPIXELS - 35, WATERFALL_W, CHAR_HEIGHT + 3, RA8875_BLACK);  // Erase waterfall in decode area
          tft.writeTo(L1);
          wfRows = WATERFALL_H - CHAR_HEIGHT - 3;
        } else {
          // erase any decoded CW and return waterfall to normal
          tft.fillRect(WATERFALL_L, YPIXELS - 35, WATERFALL_W, CHAR_HEIGHT + 3, RA8875_BLACK);  // Erase waterfall in decode area
          wfRows = WATERFALL_H;
        }
      }
      break;

    case MAIN_TUNE_INCREMENT:  // 14
      ChangeFreqIncrement(-1);
      break;

    case RESET_TUNING:  // 15
      ResetTuning();
      break;

    case UNUSED_1:  // 16
      if(radioMode == DATA_MODE) {
        switch(bands[currentBand].demod) {
          case DEMOD_PSK31:
            // try to load wav file
            if(setupPSK31Wav()) {
              // switch to play a wav file
              bands[currentBand].demod = DEMOD_PSK31_WAV;
              currentDataMode = DEMOD_PSK31_WAV;
              ShowOperatingStats();
            }
            break;

          case DEMOD_FT8:
            // *** TODO: add DEMOD_FT8_DECODE ***
            // try to load wav file
            if(SetupFT8Wav()) {
              // switch to play a wav file
              bands[currentBand].demod = DEMOD_FT8_WAV;
              currentDataMode = DEMOD_FT8_WAV;
              ShowOperatingStats();
              syncFlag = true;
              ft8State = 2;
              UpdateInfoBoxItem(IB_ITEM_FT8);
            } else {
              // couldn't load wav file
              syncFlag = false;
              ft8State = 1;
              UpdateInfoBoxItem(IB_ITEM_FT8);
            }
            break;
        }
      } else {
        // *** TODO: from v12, validate v11 calibration routines
        // changed from v11: if(calOnFlag == 0) {
        if(calibrateItem < 0) {
          ButtonFrequencyEntry();
        }
      }
      break;

    //case BEARING:  // 17
    case BEACON:     // 17
      //ButtonBearing();
      if(beaconFlag) {
        BeaconExit();
        beaconFlag = false;
      } else {
        BeaconInit();
        beaconFlag = true;
      }
      break;
  }
}

/*****
  Purpose: Error message if Select button pressed with no Menu active
*****/
FLASHMEM void NoActiveMenu() {
  tft.setFontScale((enum RA8875tsize)1);
  tft.setTextColor(RA8875_RED);
  tft.setCursor(PRIMARY_MENU_X + 1, MENUS_Y);
  tft.print("No menu selected");

  menuStatus = NO_MENUS_ACTIVE;
  mainMenuIndex = 0;
  secondaryMenuIndex = 0;
}
