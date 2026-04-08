
#include "SDT.h"

#include "Button.h"
#include "ButtonProc.h"
#include "CWProcessing.h"
#include "Display.h"
#include "EEPROM.h"
#include "Encoders.h"
#include "hardware.h"
#include "Menu.h"
#include "MenuProc.h"
#include "mouse.h"
#include "Process.h"

//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

int32_t mainMenuIndex;
int32_t secondaryMenuIndex;
int32_t subMenuMaxOptions;           // holds the number of submenu options

bool getMenuValueActive = false;
bool getMenuOptionActive = false;
bool getMenuSelected = false;
void (*ptrMenuLoop)() = NULL;
void (*ptrMenuFollowup)() = NULL;
int getMenuMin, getMenuMax, getMenuInc, getMenuOffset;
int *ptrMenuValueCurrent;

int8_t menuStatus = NO_MENUS_ACTIVE;

const char * topMenus[] = { "CW Options", "RF Options", "VFO Select",
                           "EEPROM", "AGC", "Spectrum Options", "Mic Gain", "Mic Comp",
                           "EQ Rec Set", "EQ Xmt Set", "Calibrate", "Bearing", "Beacon Monitor", "Cancel" };

void (*functionPtr[])() = { &CWOptions, &RFOptions, &VFOSelect,
                           &EEPROMOptions, &AGCOptions, &SpectrumOptions, &MicGainSet, &MicOptions,
                           &EqualizerRecOptions, &EqualizerXmtOptions, &CalibrateOptions, &BearingOptions, &BeaconOptions, &Cancel };

const char * secondaryChoices[][8] = {
  /* CW Options */ { "WPM", "Key Type", "CW Filter", "Paddle Flip", "Sidetone Vol", "Xmit Delay", "Cancel" },
  /* RF Options */ { MENU_RF_OPTIONS },
  /* VFO Select */ { "VFO A", "VFO B", "Split", "Cancel" },
  /* EEPROM */ { "Save Current", "Set Defaults", "Get Favorite", "Set Favorite", "EEPROM-->SD", "SD-->EEPROM", "SD Dump", "Cancel" },
  /* AGC */ { "Off", "Long", "Slow", "Medium", "Fast", "Cancel" },
  /* Spectrum Options */ { "20 dB/unit", "10 dB/unit", " 5 dB/unit", " 2 dB/unit", " 1 dB/unit", "Cancel" },
  /* Mic Gain */ { "Set Mic Gain", "Cancel" },
  /* Mic Comp */ //{ "On", "Off", "Set Threshold", "Set Ratio", "Set Attack", "Set Decay", "Cancel" },
  /* Mic Comp */ { "On", "Off", "Set Threshold", "Cancel" }, // only threshold is used currently
  /* EQ Rec Set */ { "On", "Off", "EQSet", "Cancel" },
  /* EQ Xmt Set */ { "On", "Off", "EQSet", "Cancel" },
  /* Calibrate */ { MENU_CAL_OPTIONS },
  /* Bearing */ { "Show Map", "Set Prefix", "Cancel" },
  /* Beacon */ { "On", "Off", "Cancel" },
  /* Cancel */ { "" }
};
const int secondaryMenuCount[] = {7, MENU_RF_COUNT, 4, 8, 6, 6, 2, 7, 4, 4, MENU_CAL_COUNT, 2, 3, 1};

const char * menuOptions[][6] = {
  /* keyChoice */ { "Straight Key", "Keyer", "Cancel" },
  /* CWFilter */  { "0.8kHz", "1.0kHz", "1.3kHz", "1.8kHz", "2.0kHz", " Off " },
  /* paddleState */ { "Right = dah", "Right = dit" }
};
const int menuOptionsCount[] = {2, 6, 2};

int receiveEQFlag;
int xmitEQFlag;

//-------------------------------------------------------------------------------------------------------------
// Code
//-------------------------------------------------------------------------------------------------------------

FLASHMEM void Cancel() {
}

/*****
  Purpose: To process a menu up or down
*****/
FLASHMEM void MenuBarChange(int change) {
  switch(menuStatus) {
    case PRIMARY_MENU_ACTIVE:
      mainMenuIndex += change;

      // limit index
      if(mainMenuIndex < 0) {
        mainMenuIndex = TOP_MENU_COUNT - 1;
      } else if(mainMenuIndex == TOP_MENU_COUNT) {
        mainMenuIndex = 0;
      }
      ShowMenu(&topMenus[mainMenuIndex], PRIMARY_MENU);
      break;

    case SECONDARY_MENU_ACTIVE:
      secondaryMenuIndex += change;

      // limit index
      if(secondaryMenuIndex < 0) {
        secondaryMenuIndex = subMenuMaxOptions - 1;
      } else if(secondaryMenuIndex == subMenuMaxOptions) {
        secondaryMenuIndex = 0;
      }
      ShowMenu(&secondaryChoices[mainMenuIndex][secondaryMenuIndex], SECONDARY_MENU);
      break;

    default:
      break;
  }
}

FLASHMEM void ShowMenuBar(int menu = 0, int change = 0) {
  if(menuStatus == NO_MENUS_ACTIVE) {
    menuStatus = PRIMARY_MENU_ACTIVE;
    mainMenuIndex = menu;
    ShowMenu(&topMenus[mainMenuIndex], PRIMARY_MENU);
  } else {
    if(change != 0) MenuBarChange(change);
  }
}

FLASHMEM void MenuBarSelect() {
  switch(menuStatus) {
    case NO_MENUS_ACTIVE:
      #ifdef DEBUG_SW
        Serial.print("NAM #0: val = ");
        Serial.println(val);
      #endif
      break;

    case PRIMARY_MENU_ACTIVE:
      if(mainMenuIndex == TOP_MENU_COUNT - 1) {
        menuStatus = NO_MENUS_ACTIVE;
        mainMenuIndex = 0;
        EraseMenus();
      } else {
        menuStatus = SECONDARY_MENU_ACTIVE;
        secondaryMenuIndex = 0;
        subMenuMaxOptions = secondaryMenuCount[mainMenuIndex];
        ShowMenu(&secondaryChoices[mainMenuIndex][secondaryMenuIndex], SECONDARY_MENU);
      }
      break;

    case SECONDARY_MENU_ACTIVE:
      if(secondaryMenuIndex == secondaryMenuCount[mainMenuIndex] - 1) {
        // cancel selected
        menuStatus = PRIMARY_MENU_ACTIVE;
        EraseSecondaryMenu();
      } else {
        functionPtr[mainMenuIndex]();

        // wrap up menu unless we're still getting a value
        if(!getMenuValueActive  && !getMenuOptionActive) {
          EraseMenus();
          menuStatus = NO_MENUS_ACTIVE;
        }
      }
      break;

    default:
      break;
  }
}

/*****
  Purpose: Get a value for a menu bar item using the encoder or mouse wheel

  Parameter list:
    int minValue                lowest value allowed
    int maxValue                largest value allowed
    int *currentValue           pointer to current value
    int increment               amount by which each increment changes the value
    char prompt[]               menu bar prompt
    void (*ptrSetup)()          pointer to function that will run at setup
    void (*ptrValue)()          pointer to function that will run at the beginning of each loop
    void (*ptrFollowup)()       pointer to function that will run after Select button is pressed or on mouse left click
*****/
FLASHMEM void GetMenuValue(int minValue, int maxValue, int *currentValue, int increment, const char *prompt, int offset, void (*ptrSetup)(), void (*ptrValue)(), void (*ptrFollowup)()) {
  getMenuMin = minValue;
  getMenuMax = maxValue;
  getMenuInc = increment;
  ptrMenuValueCurrent = currentValue;
  getMenuOffset = offset;

  getEncoderValueFlag = true;

  ShowMenuItemValue(*currentValue, offset, prompt);

  if(ptrSetup) ptrSetup();

  getMenuValueActive = true;
  getMenuSelected = false;
  ptrMenuLoop = ptrValue;
  ptrMenuFollowup = ptrFollowup;

  menuBarSelected = false;
}

// the changes made here are reflected immediately
// *** TODO: consider ability to cancel w/o change or making live update optional (that could replace GetEncoderValueLive as well ***
void GetMenuValueLoop() {
  int val = -1;
  int change = menuEncoderMove + mouseWheelValue;
  long oldValue = *ptrMenuValueCurrent;

  if(ptrMenuLoop) ptrMenuLoop();

  if(change != 0) {
    oldValue += change * getMenuInc;

    // limit value
    if(oldValue < getMenuMin) {
      oldValue = getMenuMin;
    } else if(oldValue > getMenuMax) {
      oldValue = getMenuMax;
    }

    *ptrMenuValueCurrent = oldValue;

    ShowMenuItemValue(oldValue, getMenuOffset);
  }

  // check if we're done
  if(menuBarSelected) {
    val = MENU_OPTION_SELECT;
  } else {
    val = ReadSelectedPushButton();  // Read pin that controls all switches
    if(val != BOGUS_PIN_READ) {
      val = ProcessButtonPress(val);
    }
  }

  if(val == MENU_OPTION_SELECT) {
    getMenuSelected = true;
    getEncoderValueFlag = false;
  }

  menuEncoderMove = 0;
  mouseWheelValue = 0;
}

/*****
  Purpose: Select an option from a fixed bar submenu using the Menu Up/Down button, encoder or mouse wheel

  Parameter list:
    int menuIndex               submenu options index (menuOptions)
    int numberOfChoices         number of choices available
    int *ptrCurrentValue        pointer to current value or option index
    void (*ptrSetup)()          pointer to function that will run at setup
    void (*ptrValue)()          pointer to function that will run at the beginning of each loop
    void (*ptrFollowup)()       pointer to function that will run after Select button is pressed or on mouse left click
*****/
FLASHMEM void GetMenuOption(int menuIndex, int *ptrCurrentValue, void (*ptrSetup)(), void (*ptrValue)(), void (*ptrFollowup)()) {
  getMenuOffset = menuIndex;
  ptrMenuValueCurrent = ptrCurrentValue;
  getMenuMin = 0;
  getMenuMax = menuOptionsCount[menuIndex];

  getEncoderValueFlag = true;

  ShowMenuItem(menuOptions[menuIndex][*ptrCurrentValue]);

  if(ptrSetup) ptrSetup();

  getMenuOptionActive = true;
  getMenuSelected = false;
  ptrMenuLoop = ptrValue;
  ptrMenuFollowup = ptrFollowup;

  menuBarSelected = false;
}

// the changes made here are reflected immediately
// *** TODO: consider ability to cancel w/o change or making live update optional (that could replace GetEncoderValueLive as well ***
void GetMenuOptionLoop() {
  int val = -1;
  int change = menuEncoderMove + mouseWheelValue;
  int currentValue = *ptrMenuValueCurrent;

  if(ptrMenuLoop) ptrMenuLoop();

  if(change == 0) {
    // see if a change was made through menu buttons
    val = ReadSelectedPushButton();  // Read pin that controls all switches
    if(val != BOGUS_PIN_READ) {
      val = ProcessButtonPress(val);
      if(val > -1) {                 // Valid choice?
        switch(val) {
          case MENU_OPTION_SELECT:
            val = MENU_OPTION_SELECT;
            break;

          case MAIN_MENU_UP:
            change = 1;
            val = -1;
            break;

          case MAIN_MENU_DN:
            change = -1;
            val = -1;
            break;

          default:
            val = -1;
            break;
        }
      }
    }
  }

  if(change != 0) {
    currentValue += change;

    // roll value at ends
    if(currentValue < getMenuMin) {
      currentValue = getMenuMax - 1;
    } else if(currentValue >= getMenuMax) {
      currentValue = 0;
    }

    *ptrMenuValueCurrent = currentValue;
    ShowMenuItem(menuOptions[getMenuOffset][currentValue]);
  }

  // check if an option was selected with the mouse
  if(menuBarSelected) {
    val = MENU_OPTION_SELECT;
  }

  if(val == MENU_OPTION_SELECT) {
    getMenuSelected = true;
    getEncoderValueFlag = false;
  }

  menuEncoderMove = 0;
  mouseWheelValue = 0;
}
