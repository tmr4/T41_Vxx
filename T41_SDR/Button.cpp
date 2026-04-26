#include "SDT.h"

#include "Button.h"
#include "ButtonProc.h"
#include "Display.h"
#include "EEPROM.h"
#include "ft8.h"
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
        EraseMenus();
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
      t41.SpectrumZoom += 1;
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
        EraseMenus();
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
      // change to the next standard demod mode for radio mode
      // SSB:  USB <-> LSB
      // CW:   USB <-> LSB
      // DSB:  AM -> SAM -> FM -> AM (receive only)
      // DATA: FT8 -> FT8.int -> FT8.wav
      ButtonDemodMode();
      break;

    case SET_MODE:  // 8
      // change to the next mode: SSB -> CW -> DSB -> DATA -> SSB
      ButtonMode();
      break;

    case NOISE_REDUCTION:  // 9
      ButtonNR();
      break;

    case NOTCH_FILTER:  // 10
      ButtonNotchFilter();
      UpdateInfoBoxItem(IB_ITEM_NOTCH);
      break;

    case NOISE_FLOOR:  // 11
      t41.LiveNoiseFloor += 1;
      break;

    case FINE_TUNE_INCREMENT:  // 12
      ChangeFtIncrement(1);
      break;

    case DECODER_TOGGLE:  // 13
      ToggleCWDecoder();
      break;

    case MAIN_TUNE_INCREMENT:  // 14
      ChangeFreqIncrement(-1);
      break;

    case RESET_TUNING:  // 15
      ResetTuning();
      break;

    case UNUSED_1:  // 16
      ChangeMode(DATA_MODE, DEMOD_FT8_WAV);
      //ChangeMode(DATA_MODE, DEMOD_FT8_INTERNAL);

      // *** TODO: examine restoring this ***
      // *** TODO: from v12, validate v11 calibration routines
      // changed from v11: if(calOnFlag == 0) {
      //if(calibrateItem < 0) {
      //  ButtonFrequencyEntry();
      //}
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

#ifdef DEBUG_SW
/*****
  Purpose: Error message if Select button pressed with no Menu active
*****/
FLASHMEM void NoActiveMenu() {
  Serial.println("NAM BOGUS_PIN_READ");
  menuStatus = NO_MENUS_ACTIVE;
  mainMenuIndex = 0;
  secondaryMenuIndex = 0;
}
#endif
