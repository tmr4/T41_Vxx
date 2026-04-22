
#include "..\..\SDT.h"

#include "..\..\Button.h"
#include "..\..\ButtonProc.h"
#include "Display.h"
#include "..\..\Display.h"
#include "..\..\EEPROM.h"
#include "..\..\Encoders.h"
#include "..\..\hardware.h"
#include "..\..\Filter.h"
#include "Menu.h"
#include "..\..\Menu.h"
#include "..\..\Tune.h"
#include "..\..\Utility.h"

//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

#define MAX_FAVORITES        13     // Max number of favorite frequencies stored in EEPROM

extern int TxRxFreqOld;

//-------------------------------------------------------------------------------------------------------------
// Code
//-------------------------------------------------------------------------------------------------------------

/*****
  Purpose: Direct Frequency Entry
    Base Code courtesy of Harry  GM3RVL
*****/
FLASHMEM void ButtonFrequencyEntry() {
#define show_FEHelp
  int TxRxFreq = t41.TXRXFreq();
  bool doneFE = false;                         // set to true when a valid frequency is entered
  long enteredF = 0L;                          // desired frequency
  char strF[6] = { ' ', ' ', ' ', ' ', ' ' };  // container for frequency string during entry
  String stringF;
  int valPin;
  int key;
  int numdigits = 0;  // number of digits entered
  int pushButtonSwitchIndex;
  //save_last_frequency = false;                    // prevents crazy frequencies when you change bands/save_last_frequency = true;
  // Arrays for allocating values associated with keys and switches - choose whether USB keypad or analogue switch matrix
  // USB keypad and analogue switch matrix
  const char *DE_Band[] = {"80m","40m","20m","17m","15m","12m","10m"};
  const char *DE_Flimit[] = {"4.5","9","16","26","26","30","30"};
  int numKeys[] = { 0x0D, 0x7F, 0x58,  // values to be allocated to each key push
                    0x37, 0x38, 0x39,
                    0x34, 0x35, 0x36,
                    0x31, 0x32, 0x33,
                    0x30, 0x7F, 0x7F,
                    0x7F, 0x7F, 0x99 };
  EraseMenus();
#ifdef show_FEHelp
  int keyCol[] = { YELLOW, RED, RED,
                   RA8875_BLUE, RA8875_GREEN, RA8875_GREEN,
                   RA8875_BLUE, RA8875_BLUE, RA8875_BLUE,
                   RED, RED, RED,
                   RED, RA8875_BLACK, RA8875_BLACK,
                   YELLOW, YELLOW, RA8875_BLACK };
  int textCol[] = { RA8875_BLACK, RA8875_WHITE, RA8875_WHITE,
                    RA8875_WHITE, RA8875_BLACK, RA8875_BLACK,
                    RA8875_WHITE, RA8875_WHITE, RA8875_WHITE,
                    RA8875_WHITE, RA8875_WHITE, RA8875_WHITE,
                    RA8875_WHITE, RA8875_WHITE, RA8875_WHITE,
                    RA8875_BLACK, RA8875_BLACK, RA8875_WHITE };
  const char *key_labels[] = { "<", "", "X",
                               "7", "8", "9",
                               "4", "5", "6",
                               "1", "2", "3",
                               "0", "D", "",
                               "", "", "S" };

#define KEYPAD_LEFT 350
#define KEYPAD_TOP SPECTRUM_TOP_Y + 35
#define KEYPAD_WIDTH 150
#define KEYPAD_HEIGHT 300
#define BUTTONS_LEFT KEYPAD_LEFT + 30
#define BUTTONS_TOP KEYPAD_TOP + 30
#define BUTTONS_SPACE 45
#define BUTTONS_RADIUS 15
#define TEXT_OFFSET -8

  TxRxFreqOld = TxRxFreq;
  lastFrequencies[currentBand][activeVFO] = TxRxFreq;

  tft.writeTo(L1);
  tft.fillRect(WATERFALL_L, SPECTRUM_TOP_Y + 1, WATERFALL_W, WATERFALL_BOTTOM - SPECTRUM_TOP_Y, RA8875_BLACK);  // Make space for FEInfo
  tft.fillRect(WATERFALL_W, SPEC_BOX_LABELS - 10, 15, 30, RA8875_BLACK);
  tft.writeTo(L2);

  tft.fillRect(WATERFALL_L, SPECTRUM_TOP_Y + 1, WATERFALL_W, WATERFALL_BOTTOM - SPECTRUM_TOP_Y, RA8875_BLACK);

  tft.setCursor(centerLine - 140, SPEC_BOX_LABELS);
  tft.drawRect(SPECTRUM_LEFT_X - 1, SPECTRUM_TOP_Y, WATERFALL_W + 2, 360, RA8875_YELLOW);  // Spectrum box

  // Draw keypad box
  tft.fillRect(KEYPAD_LEFT, KEYPAD_TOP, KEYPAD_WIDTH, KEYPAD_HEIGHT, DARKGREY);
  // put some circles
  tft.setFontScale((enum RA8875tsize)1);
  for(unsigned i = 0; i < 6; i++) {
    for(unsigned j = 0; j < 3; j++) {
      tft.fillCircle(BUTTONS_LEFT + j * BUTTONS_SPACE, BUTTONS_TOP + i * BUTTONS_SPACE, BUTTONS_RADIUS, keyCol[j + 3 * i]);
      tft.setCursor(BUTTONS_LEFT + j * BUTTONS_SPACE + TEXT_OFFSET, BUTTONS_TOP + i * BUTTONS_SPACE - 18);
      tft.setTextColor(textCol[j + 3 * i]);
      tft.print(key_labels[j + 3 * i]);
    }
  }
  tft.setFontScale((enum RA8875tsize)0);
  tft.setCursor(WATERFALL_L + 20, SPECTRUM_TOP_Y + 50);
  tft.setTextColor(RA8875_WHITE);
  tft.print("Direct Frequency Entry");
  tft.setCursor(WATERFALL_L + 20, SPECTRUM_TOP_Y + 100);
  tft.print("<   Apply entered frequency");
  tft.setCursor(WATERFALL_L + 20, SPECTRUM_TOP_Y + 130);
  tft.print("X   Exit without changing frequency");
  tft.setCursor(WATERFALL_L + 20, SPECTRUM_TOP_Y + 160);
  tft.print("D   Delete last digit");
  tft.setCursor(WATERFALL_L + 20, SPECTRUM_TOP_Y + 190);
  tft.print("S   Save Direct to Last Freq. ");
  tft.setCursor(WATERFALL_L + 20, SPECTRUM_TOP_Y + 240);
  tft.print("Direct Entry was called from ");
  tft.print(DE_Band[currentBand]);
  tft.print(" band");
  tft.setCursor(WATERFALL_L + 20, SPECTRUM_TOP_Y + 270);
  tft.print("Frequency response limited above ");
  tft.print(DE_Flimit[currentBand]);
  tft.print("MHz");
  tft.setCursor(WATERFALL_L + 20, SPECTRUM_TOP_Y + 300);
  tft.print("For widest direct entry frequency range");
  tft.setCursor(WATERFALL_L + 20, SPECTRUM_TOP_Y + 330);
  tft.print("call from 12m or 10m band");

#endif

  tft.writeTo(L2);

  tft.setFontScale((enum RA8875tsize)1);
  tft.setTextColor(RA8875_WHITE);
  tft.setCursor(10, 0);
  tft.print("Enter Frequency");

  tft.fillRect(SECONDARY_MENU_X + 20, MENUS_Y, EACH_MENU_WIDTH + 10, CHAR_HEIGHT, RA8875_MAGENTA);
  //tft.setTextColor(RA8875_WHITE);
  tft.setTextColor(RA8875_BLACK);       // JJP 7/17/23
  tft.setCursor(SECONDARY_MENU_X + 21, MENUS_Y);
  tft.print("kHz or MHz:");
  tft.setFontScale((enum RA8875tsize)0);
  tft.setCursor(WATERFALL_L + 50, SPECTRUM_TOP_Y + 260);
  tft.print("Save Direct to Last Freq.= ");
  tft.setCursor(WATERFALL_L + 270, SPECTRUM_TOP_Y + 190);
  if(save_last_frequency) {
    tft.setTextColor(RA8875_GREEN);
    tft.print("On");
  } else {
    tft.setTextColor(RA8875_MAGENTA);
    tft.print("Off");
  }

  while(doneFE == false) {
    valPin = ReadSelectedPushButton();                     // Poll UI push buttons
    if(valPin != BOGUS_PIN_READ) {                        // If a button was pushed...
      pushButtonSwitchIndex = ProcessButtonPress(valPin);  // Winner, winner...chicken dinner!
      key = numKeys[pushButtonSwitchIndex];
      switch(key) {
        case 0x7F:  // erase last digit =127
          if(numdigits != 0) {
            numdigits--;
            strF[numdigits] = ' ';
          }
          break;
        case 0x58:  // Exit without updating frequency =88
          doneFE = true;
          break;
        case 0x0D:  // Apply the entered frequency (if valid) =13
          stringF = String(strF);
          enteredF = stringF.toInt();
          if((numdigits == 1) || (numdigits == 2)) {
            enteredF = enteredF * 1000000;
          }
          if((numdigits == 4) || (numdigits == 5)) {
            enteredF = enteredF * 1000;
          }
          if((enteredF > 30000000) || (enteredF < 1250000)) {
            stringF = "     ";  // 5 spaces
            stringF.toCharArray(strF, stringF.length());
            numdigits = 0;
          } else {
            doneFE = true;
          }
          break;
        case 0x99:
          save_last_frequency = !save_last_frequency;
          tft.setFontScale((enum RA8875tsize)0);
          tft.fillRect(WATERFALL_L + 269, SPECTRUM_TOP_Y + 190, 50, CHAR_HEIGHT, RA8875_BLACK);
          tft.setCursor(WATERFALL_L + 260, SPECTRUM_TOP_Y + 190);
          if(save_last_frequency) {
            tft.setTextColor(RA8875_GREEN);
            tft.print("On");
          } else {
            tft.setTextColor(RA8875_MAGENTA);
            tft.print("Off");
          }
          break;
        default:
          if((numdigits == 5) || ((key == 0x30) & (numdigits == 0))) {
          } else {
            strF[numdigits] = char(key);
            numdigits++;
          }
          break;
      }
      tft.setTextColor(RA8875_WHITE);
      tft.setFontScale((enum RA8875tsize)1);
      tft.fillRect(SECONDARY_MENU_X + 195, MENUS_Y, 85, CHAR_HEIGHT, RA8875_MAGENTA);
      tft.setCursor(SECONDARY_MENU_X + 200, MENUS_Y);
      tft.print(strF);
      delay(250);  // only for analogue switch matrix
    }
  }

  if(key != 0x58) {
    t41.CenterFreq = enteredF;
  }

  t41.NCOFreq = 0L;
  directFreqFlag = true;
  fineTuneFlag = true;  // Put back in so tuning bar is refreshed
  SetFreq(t41.CenterFreq);  // Used here instead of fineTuneFlag

  if(save_last_frequency) {
    lastFrequencies[currentBand][activeVFO] = enteredF;
  } else {
    lastFrequencies[currentBand][activeVFO] = TxRxFreqOld;
  }
  tft.fillRect(0, 0, 799, 479, RA8875_BLACK);   // Clear layer 2
  tft.writeTo(L1);

  EEPROMWrite();

  SetBand(t41.CenterFreq);
  RedrawDisplayScreen(); // *** we can get rid of this by adjusting above to not write to right portion of screen ***
}

/*****
  Purpose: Used to save a favortie frequency to EEPROM

  Parameter list:

  CAUTION: This code assumes you have set the curently active VFO frequency to the new
           frequency you wish to save. You them use the menu encoder to scroll through
           the current list of stored frequencies. Stop on the one that you wish to
           replace and press Select to save in EEPROM. The currently active VFO frequency
           is then stored to EEPROM.
*****/
FLASHMEM void SetFavoriteFrequency() {
  int index;
  int val;
  int TxRxFreq = t41.TXRXFreq();

  tft.setFontScale((enum RA8875tsize)1);

  index = 0;
  tft.setTextColor(RA8875_WHITE);
  tft.fillRect(SECONDARY_MENU_X, MENUS_Y, EACH_MENU_WIDTH, CHAR_HEIGHT, RA8875_MAGENTA);
  tft.setCursor(SECONDARY_MENU_X, MENUS_Y);
  tft.print(favoriteFreqs[index]);
  while(true) {
    if(menuEncoderMove != 0) {  // Changed encoder?
      index += menuEncoderMove;  // Yep
      if(index < 0) {
        index = MAX_FAVORITES - 1;  // Wrap to last one
      } else {
        if(index > MAX_FAVORITES)
          index = 0;  // Wrap to first one
      }
      tft.fillRect(SECONDARY_MENU_X, MENUS_Y, EACH_MENU_WIDTH, CHAR_HEIGHT, RA8875_MAGENTA);
      tft.setCursor(SECONDARY_MENU_X, MENUS_Y);
      tft.print(favoriteFreqs[index]);
      menuEncoderMove = 0;
    }

    val = ReadSelectedPushButton();  // Read pin that controls all switches
    val = ProcessButtonPress(val);
    delay(150L);
    if(val == MENU_OPTION_SELECT) {  // Make a choice??
      EraseMenus();
      favoriteFreqs[index] = TxRxFreq;

      if(activeVFO == VFO_A) {
        t41.CurrentFreqA = TxRxFreq;
      } else {
        t41.CurrentFreqB = TxRxFreq;
      }

      SetFreq(TxRxFreq);
      ShowOperatingStats();
      ShowBandwidthBarValues();
      CalcFilters();
      ShowFrequency();
      break;
    }
  }
}

/*****
  Purpose: Used to fetch a favortie frequency as stored in EEPROM. It then copies that
           frequency to the currently active VFO

  Parameter list:
*****/
FLASHMEM void GetFavoriteFrequency() {
  int index = 0;
  int val;
  int currentBand2 = 0;
  int centerFreq;
  int TxRxFreq = t41.TXRXFreq();

  tft.setFontScale((enum RA8875tsize)1);

  tft.setTextColor(RA8875_WHITE);
  tft.fillRect(SECONDARY_MENU_X, MENUS_Y, EACH_MENU_WIDTH, CHAR_HEIGHT, RA8875_MAGENTA);
  tft.setCursor(SECONDARY_MENU_X, MENUS_Y);
  tft.print(favoriteFreqs[index]);
  while(true) {
    if(menuEncoderMove != 0) {  // Changed encoder?
      index += menuEncoderMove;  // Yep
      if(index < 0) {
        index = MAX_FAVORITES - 1;  // Wrap to last one
      } else {
        if(index > MAX_FAVORITES)
          index = 0;  // Wrap to first one
      }
      tft.fillRect(SECONDARY_MENU_X, MENUS_Y, EACH_MENU_WIDTH, CHAR_HEIGHT, RA8875_MAGENTA);
      tft.setCursor(SECONDARY_MENU_X, MENUS_Y);
      tft.print(favoriteFreqs[index]);
      menuEncoderMove = 0;
    }

    val = ReadSelectedPushButton();  // Read pin that controls all switches
    val = ProcessButtonPress(val);
    delay(150L);

    centerFreq = favoriteFreqs[index];  // current frequency
    if(centerFreq >= bands[BAND_80M].fBandLow && centerFreq <= bands[BAND_80M].fBandHigh) {
      currentBand2 = BAND_80M;
    } else if(centerFreq >= bands[BAND_80M].fBandHigh && centerFreq <= 7000000L) {  // covers 5MHz WWV AFP 11-03-22
      currentBand2 = BAND_80M;
    } else if(centerFreq >= bands[BAND_40M].fBandLow && centerFreq <= bands[BAND_40M].fBandHigh) {
      currentBand2 = BAND_40M;
    } else if(centerFreq >= bands[BAND_40M].fBandHigh && centerFreq <= 14000000L) {  // covers 10MHz WWV AFP 11-03-22
      currentBand2 = BAND_40M;
    } else if(centerFreq >= bands[BAND_20M].fBandLow && centerFreq <= bands[BAND_20M].fBandHigh) {
      currentBand2 = BAND_20M;
    } else if(centerFreq >= 14000000L && centerFreq <= 18000000L) {  // covers 15MHz WWV AFP 11-03-22
      currentBand2 = BAND_20M;
    } else if(centerFreq >= bands[BAND_17M].fBandLow && centerFreq <= bands[BAND_17M].fBandHigh) {
      currentBand2 = BAND_17M;
    } else if(centerFreq >= bands[BAND_15M].fBandLow && centerFreq <= bands[BAND_15M].fBandHigh) {
      currentBand2 = BAND_15M;
    } else if(centerFreq >= bands[BAND_12M].fBandLow && centerFreq <= bands[BAND_12M].fBandHigh) {
      currentBand2 = BAND_12M;
    } else if(centerFreq >= bands[BAND_10M].fBandLow && centerFreq <= bands[BAND_10M].fBandHigh) {
      currentBand2 = BAND_10M;
    }
    currentBand = currentBand2;


    if(val == MENU_OPTION_SELECT) {  // Make a choice??
      t41.CenterFreq = centerFreq;
      switch(activeVFO) {
        case VFO_A:
          if(currentBandA == NUMBER_OF_BANDS) {  // Incremented too far?
            currentBandA = 0;                     // Yep. Roll to list front.
          }
          currentBandA = currentBand2;
          lastFrequencies[currentBand][VFO_A] = TxRxFreq;
          break;

        case VFO_B:
          if(currentBandB == NUMBER_OF_BANDS) {  // Incremented too far?
            currentBandB = 0;                     // Yep. Roll to list front.
          }                                       // Same for VFO B
          currentBandB = currentBand2;
          lastFrequencies[currentBand][VFO_B] = TxRxFreq;
          break;
      }
    }
    if(val == MENU_OPTION_SELECT) {

      //EraseSpectrumDisplayContainer();
      //DrawSpectrumFrame();
      //ShowSpectrumFreqValues();
      //SetBand();
      //ShowSpectrumdBScale();
      //EraseMenus();
      //ResetTuning();
      //ShowOperatingStats();
      //t41.NCOFreq = 0L;
      //DrawBandwidthBar();  // AFP 10-20-22
      //digitalWrite(bandswitchPins[currentBand], LOW);
      //ShowSpectrumdBScale();
      //DrawFreqSpectrum();
      //currentDemodMode = currentBand;
      return;
    }
  }
}
