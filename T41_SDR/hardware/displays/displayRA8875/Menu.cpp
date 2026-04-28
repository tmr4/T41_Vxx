
#include "..\..\SDT.h"

#include "..\..\Button.h"
#include "Display.h"
#include "..\..\Display.h"
#include "..\..\EEPROM.h"
#include "..\..\Encoders.h"
#include "..\..\hardware.h"
#include "Menu.h"
#include "..\..\Menu.h"

//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

#define EQUALIZER_CELL_COUNT     14

//-------------------------------------------------------------------------------------------------------------
// Code
//-------------------------------------------------------------------------------------------------------------

/*****
  Purpose: Display top line menu according to set menu parameters

  Parameter list:
    char *menuItem          pointers to the menu
    int where               PRIMARY_MENU or SECONDARY_MENU
*****/
FLASHMEM void ShowMenu(const char *menu[], int where) {
  tft.setFontScale( (enum RA8875tsize) 1);

  if(menuStatus == NO_MENUS_ACTIVE) {
    Serial.println("NAM #4");
  }

  switch(where) {
    case PRIMARY_MENU:
      // reduce waterfall height to make room for menu
      SetWaterfallHeight(32); // (CHAR_HEIGHT);

      tft.fillRect(PRIMARY_MENU_X, MENUS_Y, EACH_MENU_WIDTH, CHAR_HEIGHT, RA8875_BLUE);
      tft.setCursor(PRIMARY_MENU_X + 1, MENUS_Y);
      tft.setTextColor(RA8875_WHITE);
      tft.print(*menu);
      break;

    case SECONDARY_MENU:
      tft.fillRect(SECONDARY_MENU_X, MENUS_Y, EACH_MENU_WIDTH, CHAR_HEIGHT, RA8875_GREEN);
      tft.setCursor(SECONDARY_MENU_X + 1, MENUS_Y);
  //    tft.setTextColor(RA8875_WHITE);
      tft.setTextColor(RA8875_BLACK);
      tft.print(*menu);  // Secondary Menu
      break;

      default:
        break;
  }
}

FLASHMEM void ShowMenuItem(const char *item) {
  tft.setFontScale((enum RA8875tsize)1);

  // erase old menu
  tft.fillRect(SECONDARY_MENU_X, MENUS_Y, EACH_MENU_WIDTH, CHAR_HEIGHT, RA8875_GREEN);

  // update current menu
  tft.setTextColor(RA8875_BLACK);
  tft.setCursor(SECONDARY_MENU_X, MENUS_Y);
  tft.print(item);
}

FLASHMEM void ShowMenuItemValue(int value, int offset /* = 0 */, const char *prompt /* = NULL */) {
  tft.setFontScale((enum RA8875tsize)1);
  tft.setTextColor(RA8875_WHITE);

  // show prompt
  if(prompt != NULL) {
    // erase old menu
    tft.fillRect(SECONDARY_MENU_X, MENUS_Y, EACH_MENU_WIDTH, CHAR_HEIGHT, RA8875_MAGENTA);

    tft.setCursor(SECONDARY_MENU_X, MENUS_Y);
    tft.print(prompt);
  } else {
    // erase old value
    tft.fillRect(SECONDARY_MENU_X + offset, MENUS_Y, EACH_MENU_WIDTH, CHAR_HEIGHT, RA8875_MAGENTA);
  }

  // update current value
  tft.setCursor(SECONDARY_MENU_X + offset, MENUS_Y);
  tft.print(value);
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
      yLevel[iFreq] = t41.equalizerRx[iFreq];
    } else {
      if(EQType == 1) {
        yLevel[iFreq] = t41.equalizerTx[iFreq];
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
          t41.equalizerRx[columnIndex] = newValue;
          t41.equalizerRx[columnIndex] = t41.equalizerRx[columnIndex];
        } else {
          if(EQType == 1) {
            t41.equalizerTx[columnIndex] = newValue;
            t41.equalizerTx[columnIndex] = t41.equalizerTx[columnIndex];
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
