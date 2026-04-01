
#include "..\..\SDT.h"

#include "..\..\Button.h"
#include "Display.h"
#include "..\..\Display.h"
#include "..\..\Encoders.h"
#include "Menu.h"
#include "..\..\Menu.h"
#include "..\..\Process.h"

/*****
  Purpose: To present the encoder-driven full menu display
*****/
/*
const char *secondaryFunctions[][8] = {
  { "WPM", "Key Type", "CW Filter", "Paddle Flip", "Sidetone Vol", "Xmit Delay", "Cancel" },
  { "Power level", "Gain", "Cancel" },
  { "VFO A", "VFO B", "Split", "Cancel" },
  { "Save Current", "Set Defaults", "Get Favorite", "Set Favorite", "EEPROM-->SD", "SD-->EEPROM", "SD Dump", "Cancel" },
  { "Off", "Long", "Slow", "Medium", "Fast", "Cancel" },
  { "20 dB/unit", "10 dB/unit", " 5 dB/unit", " 2 dB/unit", " 1 dB/unit", "Cancel" },
  { "Set floor", "Cancel" },
  { "Set Mic Gain", "Cancel" },
  { "On", "Off", "Set Threshold", "Set Ratio", "Set Attack", "Set Decay", "Cancel" },
  { "On", "Off", "EQSet", "Cancel" },
  { "On", "Off", "EQSet", "Cancel" },
  { "Freq Cal", "CW PA Cal", "Rec Cal", "Xmit Cal", "SSB PA Cal", "Cancel" },
  { "Set Prefix", "Cancel" }
};
*/
FLASHMEM void DrawMenuDisplay() {
  menuStatus = 0;                                                       // No primary or secondary menu set
  mainMenuIndex = 0;
  secondaryMenuIndex = 0;

  tft.writeTo(L2);                                                      // Clear layer 2.  KF5N July 31, 2023
  tft.clearMemory();
  tft.writeTo(L1);
  tft.fillRect(1, SPECTRUM_TOP_Y + 1, 513, 379, RA8875_BLACK);          // Show Menu box
  tft.drawRect(1, SPECTRUM_TOP_Y + 1, 513, 378, RA8875_YELLOW);

  tft.setFontScale((enum RA8875tsize)1);
  tft.setTextColor(RA8875_WHITE);
  for(int i = 0; i < TOP_MENU_COUNT; i++) {                                // Show primary menu list
    tft.setCursor(10, i * 25 + 115);
    tft.print(topMenus[i]);
  }

  tft.setTextColor(RA8875_GREEN);                                       // show currently active menu
  tft.setCursor(10, mainMenuIndex * 25 + 115);
  tft.print(topMenus[mainMenuIndex]);

  tft.setTextColor(DARKGREY, RA8875_BLACK);
  for(int i = 0; i < secondaryMenuCount[mainMenuIndex]; i++) {                                // Show primary menu list
    tft.setCursor(300, i * 27 + 115);
    tft.print(secondaryChoices[mainMenuIndex][i]);
  }
}

/*****
  Purpose: To select the primary menu on full menu
*****/
FLASHMEM void SetPrimaryMenuIndex() {
  int val;

  while(true) {
    UpdateClock();
    DrawFreqSpectrum();
    DrawAudioSpectrum();

    // update menu on menu encoder move
    if(menuEncoderMove != 0) {
      // unhighlight current menu selection
      tft.setFontScale((enum RA8875tsize)1);
      tft.setTextColor(RA8875_WHITE);
      tft.setCursor(10, mainMenuIndex * 25 + 115);
      tft.print(topMenus[mainMenuIndex]);

      // update and limit menu index
      mainMenuIndex += menuEncoderMove;
      if(mainMenuIndex >= TOP_MENU_COUNT) {
        mainMenuIndex = 0;
      } else if(mainMenuIndex < 0) {
        mainMenuIndex = TOP_MENU_COUNT - 1;
      }

      // highlight selection
      tft.setTextColor(RA8875_GREEN);
      tft.setCursor(10, mainMenuIndex * 25 + 115);
      tft.print(topMenus[mainMenuIndex]);

      // update secondary menu
      tft.fillRect(299, SPECTRUM_TOP_Y + 5, 210, 279, RA8875_BLACK);
      tft.setTextColor(DARKGREY);
      for(int i = 0; i < secondaryMenuCount[mainMenuIndex]; i++) {
        tft.setCursor(300, i * 25 + 115);
        tft.print(secondaryChoices[mainMenuIndex][i]);
      }

      menuEncoderMove = 0;
    }

    val = ReadSelectedPushButton();

    YieldForProcess(150L);

    if(val != BOGUS_PIN_READ) { // If a button was pushed...
      val = ProcessButtonPress(val);
      if(val > -1) { // Valid choice?
        if(val == MENU_OPTION_SELECT) {
          break;
        }

        YieldForProcess(50L);
      }
    }
  }
}

/*****
  Purpose: To select the secondary menu on full menu
*****/
FLASHMEM void SetSecondaryMenuIndex() {
  int index = 0;
  int oldIndex = 0;
  int val;

  secondaryMenuIndex = 0;

  // highlight secondary menu item
  tft.setFontScale((enum RA8875tsize)1);
  tft.setTextColor(RA8875_GREEN);
  tft.setCursor(300, 115);
  tft.print(secondaryChoices[mainMenuIndex][0]);

  while(true) {
    YieldToProcess();

    // update menu on menu encoder move
    if(menuEncoderMove != 0) {
      // unhighlight current menu selection
      tft.setFontScale((enum RA8875tsize)1);
      tft.setTextColor(DARKGREY);
      tft.setCursor(300, oldIndex * 25 + 115);
      tft.print(secondaryChoices[mainMenuIndex][oldIndex]);

      // update and limit secondary menu index
      index += menuEncoderMove;
      if(index == secondaryMenuCount[mainMenuIndex]) {
        index = 0;
      } else if(index < 0) {
        index = secondaryMenuCount[mainMenuIndex] - 1;
      }

      oldIndex = index;

      // highlight secondary menu item
      tft.setTextColor(RA8875_GREEN);
      tft.setCursor(300, index * 25 + 115);
      tft.print(secondaryChoices[mainMenuIndex][index]);

      menuEncoderMove = 0;
    }

    val = ReadSelectedPushButton();

    YieldForProcess(200L);

    if(val != BOGUS_PIN_READ) { // If a button was pushed...
      val = ProcessButtonPress(val);
      if(val > -1) { // Valid choice?
        if(val == MENU_OPTION_SELECT) {
          secondaryMenuIndex = index;
          break;
        }

        YieldForProcess(50L);
      }
    }
  }
}
