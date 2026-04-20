
#include <malloc.h>
#include <Metro.h>
#include <TimeLib.h>                   // Part of Teensy Time library

#include "..\..\SDT.h"

#include "..\..\Button.h"
#include "..\..\ButtonProc.h"
#include "..\..\CWProcessing.h"
#include "Display.h"
#include "..\..\Display.h"
#include "..\..\EEPROM.h"
#include "..\..\Encoders.h"
#include "..\..\ft8.h"
#include "InfoBox.h"
#include "..\..\keyer.h"
#include "Menu.h"
#include "..\..\Menu.h"
#include "..\..\mouse.h"
#include "..\..\Process.h"
#include "..\..\Tune.h"
#include "..\..\Utility.h"

//-------------------------------------------------------------------------------------------------------------
// Forwards
//-------------------------------------------------------------------------------------------------------------

void IBDecoderFollowup(int row, int col);
void IBCompressionFollowup(int row, int col);
void IBTuneIncFollowup(int row, int col);
void IBWPMFollowup(int row, int col);
void IBVolFollowup(int row, int col);
void IBEQFollowup(int row, int col);
void IBTempFollowup(int row, int col);
void IBLoadFollowup(int row, int col);
void IBFT8Followup(int row, int col);
void IBFT8RxTxFollowup(int row, int col);
void IBKeyerFollowup(int row, int col);
void IBStackFollowup(int row, int col);
void IBHeapFollowup(int row, int col);
void IBRFGainFollowup(int row, int col);

void ClearInfoBox();

//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

Metro ms_500 = Metro(500); // display clock updates

typedef  struct {
  const char *label;      // info box label
  const char **options;   // label options
  int *option;            // pointer to option selector or pointer to the actual value if options is NULL
  int fontSize;           // 0 - small or 1 - large font (large font takes two rows, adjust item rows and/or IB_ROW_#_Y accordingly)
  int clearWidth;         // maximum number of characters to clear when updating field
  int highlightFlag;      // 0 - highlight all options in green, 1 - don't highlight first option, 2 - first option white, second option red, other options green

  // specifying row and col index is easiest but less flexible especailly if you use both small and large fonts
  // as in the fefault info box
  //int col, row;           // item column and row (up to 10 rows, 2 columns)
  int col, row;           // item placement by screen pixel (up to 10 rows with small font)
  void (*followFnPtr)(int row, int col);  // function to run after info box field is updated (note that these may be hard-coded to a particular location
                          // and will need updated if the underlying item is moved)
} infoBoxItem;

#define IB_COL_1_X        INFO_BOX_L + 90  // X coordinate for info box 1st column field
#define IB_COL_2_X        INFO_BOX_L + 220 // X coordinate for info box 2nd column field
#define IB_COL_2L_X       INFO_BOX_L + 205 // X coordinate for info box 2nd column field

// 14 rows possible with current spacing
#define IB_ROW_1_Y        INFO_BOX_T + 20
#define IB_ROW_2_Y        IB_ROW_1_Y + 12 // extra padding, not useable if row 1 is large
#define IB_ROW_3_Y        IB_ROW_2_Y + 20
#define IB_ROW_4_Y        IB_ROW_3_Y + 20
#define IB_ROW_5_Y        IB_ROW_4_Y + 20
#define IB_ROW_6_Y        IB_ROW_5_Y + 20
#define IB_ROW_7_Y        IB_ROW_6_Y + 20
#define IB_ROW_8_Y        IB_ROW_7_Y + 20
#define IB_ROW_9_Y        IB_ROW_8_Y + 20
#define IB_ROW_10_Y       IB_ROW_9_Y + 20
#define IB_ROW_11_Y       IB_ROW_10_Y + 20
#define IB_ROW_12_Y       IB_ROW_11_Y + 20
#define IB_ROW_13_Y       IB_ROW_12_Y + 20
#define IB_ROW_14_Y       IB_ROW_13_Y + 20
//#define IB_ROW_15_Y       IB_ROW_14_Y + 20

#define DECODER_WPM_X     IB_COL_1_X + 37

const char *agcOpts[] = { "Off", "L", "S", "M", "F" };
const char *tuneValues[] = { "10", "50", "100", "250", "1000", "10000", "100000", "1000000" };
const char *ftValues[] = { "10", "50", "250", "500" };
const char *filter[] = { "Off", "Kim", "Spectral", "LMS" };
const char *onOff[2] = { "Off", "On" };
const char *nfOptions[3] = { "Off", "Auto", "On"};
const char *zoomOptions[] = { "1x ", "2x ", "4x ", "8x ", "16x" }; // combine with MAX_ZOOM_ENTRIES somewhere

const char *keyerOpts[] = { "Off", "WPM" };
const char *optionsWPM[2] = { "Straight Key", "Paddles " };

const char *ft8Opts[] = { "no sync", "sync" };
const char *ft8TxOpts[] = { "Off", "enabled" };
const char *ft8IntOpts[] = { "even", "odd" };
const char *ft8CqOpts[] = { "man", "auto" };

#define IB_NUM_ITEMS 24

// true = info box is active and shown
bool infoBoxItemActive[IB_NUM_ITEMS] = {
  true,  // Vol
  true,  // AGC
  true,  // CT Inc
  true,  // FT Inc
  true,  // Zoom
  true,  // Noise Floor
  true,  // Auto Notch
  true,  // Compress
  true,  // Noise Filter
  true,  // RF Gain
  true,  // Equalizers
  false, // Decoder
  false, // Key Type
  false, // Keyer
  false, // FT8 sync
  false, // FT8 Tx interval
  false, // FT8 Tx enabled
  false, // FT8 Tx interval
  false, // FT8 Tx freq
  false, // FT8 Rx freq
  true,  // Stack
  true,  // Heap
  true,  // Teensy Temp
  true   // Teensy Load
};

// *** TODO: add version ***
/* PROGMEM */ const infoBoxItem infoBox[] =
{ //                                                     font    # chars
  // label         options      option                   size    to erase  flag  col            row,           follow-up function
  { "Vol:",        NULL,        (int*)&t41.AudioVolume,             1,        3,      0,   IB_COL_1_X,    IB_ROW_1_Y,    &IBVolFollowup         }, // Vol
  { "AGC",         agcOpts,     &AGCMode,                 1,        3,      1,   IB_COL_2L_X,   IB_ROW_1_Y,    NULL                   }, // AGC
  { "CT Inc:",     tuneValues,  &tuneIndex,               0,        7,      0,   IB_COL_1_X,    IB_ROW_3_Y,    &IBTuneIncFollowup     }, // CT Inc
  { "FT Inc:",     ftValues,    &ftIndex,                 0,        3,      0,   IB_COL_2_X,    IB_ROW_3_Y,    &IBTuneIncFollowup     }, // FT Inc
  { "Zoom:",       zoomOptions, (int*)&spectrumZoom,      0,        3,      0,   IB_COL_1_X,    IB_ROW_4_Y,    NULL                   }, // Zoom
  { "NF Set:",     nfOptions,   &liveNoiseFloorFlag,      0,        4,      1,   IB_COL_2_X,    IB_ROW_4_Y,    NULL                   }, // Noise Floor
  { "AutoNotch:",  onOff,       (int*)&ANR_notchOn,       0,        3,      1,   IB_COL_1_X,    IB_ROW_5_Y,    NULL                   }, // Auto Notch
  { "Compress:",   onOff,       &compressorFlag,          0,        6,      1,   IB_COL_2_X,    IB_ROW_5_Y,    &IBCompressionFollowup }, // Compress

  // Noise needs to be in column 1
  { "Noise:",      filter,      &nrOptionSelect,          0,        8,      1,   IB_COL_1_X,    IB_ROW_6_Y,    NULL                   }, // Noise Filter
  { "RF Gain:",    NULL,        NULL,                     0,        3,      0,   IB_COL_2_X,    IB_ROW_6_Y,    &IBRFGainFollowup      }, // RF Gain

  // Equalizers takes two columns
  { "Equalizer:",  NULL,        NULL,                     0,       10,      1,   IB_COL_1_X,    IB_ROW_7_Y,    &IBEQFollowup          }, // Equalizers

  // Decider takes two columns
  { "Decoder:",    onOff,       &decoderFlag,             0,        3,      1,   IB_COL_1_X,    IB_ROW_8_Y,    NULL                   }, // Decoder

  // Key type takes two columns
  { "Key Type:",   optionsWPM,  &keyType,      0,       12,      0,   IB_COL_1_X,    IB_ROW_9_Y,    &IBWPMFollowup         }, // Key Type

  // Memory keyer requires 3 rows
  { "Keyer     ",  keyerOpts,   &keyerState,              0,       10,      1,   IB_COL_1_X,    IB_ROW_10_Y,    &IBKeyerFollowup      }, // Keyer

  { "FT8       ",  ft8Opts,     &ft8SyncState,            0,        8,      1,   IB_COL_1_X,    IB_ROW_10_Y,   NULL                   }, // FT8 sync
  { "Tx Int:",     ft8IntOpts,  &ft8IntState,             0,        4,      0,   IB_COL_2_X,    IB_ROW_10_Y,   NULL                   }, // FT8 Tx interval
  { "Tx:",         ft8TxOpts,   &ft8TxState,              0,        7,      1,   IB_COL_1_X,    IB_ROW_11_Y,   NULL                   }, // FT8 Tx enabled
  { "CQ resp:",    ft8CqOpts,   &ft8CqState,              0,        4,      1,   IB_COL_2_X,    IB_ROW_11_Y,   NULL                   }, // FT8 Tx interval
  { "Tx Freq:",    NULL,        &ft8TxFreq,               0,        5,      0,   IB_COL_1_X,    IB_ROW_12_Y,   &IBFT8RxTxFollowup     }, // FT8 Tx freq
  { "Rx Freq:",    NULL,        &ft8RxFreq,               0,        5,      0,   IB_COL_2_X,    IB_ROW_12_Y,   &IBFT8RxTxFollowup     }, // FT8 Rx freq

  { "Stack:",      NULL,        NULL,                     0,        4,      2,   IB_COL_1_X,    IB_ROW_13_Y,   &IBStackFollowup       }, // Stack
  { "Heap:",       NULL,        NULL,                     0,        4,      2,   IB_COL_2_X,    IB_ROW_13_Y,   &IBHeapFollowup        }, // Heap
  { "Temp:",       NULL,        NULL,                     0,        3,      1,   IB_COL_1_X,    IB_ROW_14_Y,   &IBTempFollowup        }, // Teensy Temp
  { "Load:",       NULL,        NULL,                     0,        4,      1,   IB_COL_2_X,    IB_ROW_14_Y,   &IBLoadFollowup        }  // Teensy Load
};

//-------------------------------------------------------------------------------------------------------------
// Code
//-------------------------------------------------------------------------------------------------------------

/*****
  Purpose: Updates the specified information box item

  Parameter list:
    infoBoxItem *item   Pointer to the info box item to update
*****/
//void UpdateInfoBoxItem(uint8_t item) {
void UpdateInfoBoxItem(int item) {
  int label_x;
//  int xOffset = infoBox[item].col == 1 ? IB_COL_1_X : IB_COL_2_X;
//  int yOffset = IB_ROW_1_Y + (infoBox[item].row - 1) * 20;
  int xOffset = infoBox[item].col;
  int yOffset = infoBox[item].row;

  // *** TODO: warning the following could be breaking for displays other than the T41 operating display ***
  //if(displayState == DISPLAY_T41)
  {
    if(!infoBoxItemActive[item] || (item >= IB_NUM_ITEMS)) return;

    //if(item == IB_ITEM_TUNE) Serial.println(tuneIndex);

    tft.setFontScale((enum RA8875tsize)infoBox[item].fontSize);
    tft.fillRect(xOffset, yOffset, tft.getFontWidth() * infoBox[item].clearWidth, tft.getFontHeight(), RA8875_BLACK);
    tft.setTextColor(RA8875_WHITE);
    label_x = xOffset - 5 - strlen(infoBox[item].label) * tft.getFontWidth();
    tft.setCursor(label_x, yOffset);
    tft.print(infoBox[item].label);

    if(infoBox[item].options != NULL) {
      if((infoBox[item].highlightFlag > 0) && (*infoBox[item].option == 0)) {
        tft.setTextColor(RA8875_WHITE);
      } else if((infoBox[item].highlightFlag == 2) && (*infoBox[item].option == 1)) {
        tft.setTextColor(RA8875_RED);
      } else {
        tft.setTextColor(RA8875_GREEN);
      }

      tft.setCursor(xOffset, yOffset);
      tft.print(infoBox[item].options[*infoBox[item].option]);
    }

    if(infoBox[item].followFnPtr != NULL) {
      infoBox[item].followFnPtr(infoBox[item].row, infoBox[item].col);
    }
  }
}

// *** assumes blank region ***
void ShowVersion() {
  tft.setFontScale((enum RA8875tsize) 0);
  tft.setCursor(TIME_X + 18 * tft.getFontWidth(), TIME_Y);
  tft.setTextColor(YELLOW);
  tft.print(VERSION);
}

/*****
  Purpose: Updates the information box
*****/
void UpdateInfoBox() {
  ClearInfoBox();
  ShowVersion();

  // you can update each item individually if they need done in a particular order ...
  //UpdateInfoBoxItem(IB_ITEM_VOL);
  //UpdateInfoBoxItem(IB_ITEM_AGC);
  //UpdateInfoBoxItem(IB_ITEM_TUNE);
  //UpdateInfoBoxItem(IB_ITEM_FINE);
  //UpdateInfoBoxItem(IB_ITEM_COMPRESS);
  //UpdateInfoBoxItem(IB_ITEM_DECODER);
  //UpdateInfoBoxItem(IB_ITEM_FILTER);
  //UpdateInfoBoxItem(IB_ITEM_FLOOR);
  //UpdateInfoBoxItem(IB_ITEM_NOTCH);
  //UpdateInfoBoxItem(IB_ITEM_KEY);
  //UpdateInfoBoxItem(IB_ITEM_ZOOM);

  // ... or update them in order
  for(int i = 0; i < IB_NUM_ITEMS; i++) {
    UpdateInfoBoxItem(i);
  }
}

void ClearInfoBoxRow(int row) {
  tft.fillRect(INFO_BOX_L + 2, row,  INFO_BOX_W - 6, 20, RA8875_BLACK);
}

/*****
  Purpose: Information box follow up function for the Compression item

  Parameter list:
    int row, col  Row and column of info box item
*****/
void IBTuneIncFollowup(int row, int col) {
  SetFtActive(!mouseCenterTuneActive);
}

/*****
  Purpose: Information box follow up function for the Compression item
           Assumes this is only called as part of updating Compression item

  Parameter list:
    int row, col  Row and column of info box item
*****/
void IBCompressionFollowup(int row, int col) {
  if(compressorFlag == 1) {
    tft.print(" ");
    tft.print(currentMicThreshold);
  }
}

/*****
  Purpose: Information box follow up function for the Keyer item
           Assumes this is only called as part of updating Keyer item

  Parameter list:
    int row, col  Row and column of info box item
*****/
void IBWPMFollowup(int row, int col) {
  if(keyType == 1) { // 1 = paddles
    if(paddleFlip == 0) {
      tft.print("R");
    } else {
      tft.print("L");
    }
    tft.print(" ");
    tft.print(currentWPM);
  }
}

/*****
  Purpose: Information box follow up function for the Volume item
           Assumes volume is in column 1 row 1, with large font

  Parameter list:
    int row, col  Row and column of info box item
*****/
void IBVolFollowup(int row, int col) {
  tft.setFontScale((enum RA8875tsize)1);
  tft.setTextColor(RA8875_GREEN);
  tft.setCursor(col, row);
  tft.print(t41.AudioVolume);
}

/*****
  Purpose: Information box follow up function for the Volume item
           Assumes volume is in column 1 row 1, with large font

  Parameter list:
    int row, col  Row and column of info box item
*****/
void IBRFGainFollowup(int row, int col) {
  tft.setFontScale((enum RA8875tsize)0);
  tft.setTextColor(RA8875_GREEN);
  tft.setCursor(col, row);
  tft.print(rfGainAllBands);
}

/*****
  Purpose: Information box follow up function for the Equalizers item
           Assumes Equalizers are in column 1 row 10

  Parameter list:
    int row, col  Row and column of info box item
*****/
void IBEQFollowup(int row, int col) {
  tft.setCursor(col, row);
  if(receiveEQFlag) {
    tft.setTextColor(RA8875_RED);
    tft.print("Rx");
    tft.setTextColor(RA8875_GREEN);
    tft.setCursor(col + 25, row);
    tft.print("On");
  } else {
    tft.setTextColor(RA8875_RED);
    tft.print("Rx");
    tft.setCursor(col + 25, row);
    tft.setTextColor(RA8875_WHITE);
    tft.print("Off");
  }
  tft.setCursor(col + 55, row);
  if(xmitEQFlag) {
    tft.setTextColor(RA8875_RED);
    tft.print("Tx");
    tft.setTextColor(RA8875_GREEN);
    tft.setCursor(col + 80, row);
    tft.print("On");
  } else {
    tft.setTextColor(RA8875_RED);
    tft.print("Tx");
    tft.setTextColor(RA8875_WHITE);
    tft.setCursor(col + 80, row);
    tft.print("Off");
  }
}

/*****
  Purpose: Information box follow up function for the Temp item

  Parameter list:
    int row, col  Row and column of info box item
*****/
void IBTempFollowup(int row, int col) {
  char buff[10];

  tft.setFontScale((enum RA8875tsize)0);
  tft.setTextColor(RA8875_GREEN);
  MyDrawFloatP(TGetTemp(), 0, col, row, buff, 2);
  tft.drawCircle(col + 22, row + 5, 3, RA8875_GREEN);
}

/*****
  Purpose: Information box follow up function for the Load item

  Parameter list:
    int row, col  Row and column of info box item
*****/
void IBLoadFollowup(int row, int col) {
  char buff[10];
  int valueColor = RA8875_GREEN;
  double block_time;
  double processor_load;
  double elapsed_micros_mean;

  elapsed_micros_mean = elapsed_micros_sum / elapsed_micros_idx_t;

  block_time = 128.0 / sampleRate;  // one audio block is 128 samples and uses this in seconds
  block_time = block_time * 16;

  block_time *= 1000000.0;                                  // now in µseconds
  processor_load = elapsed_micros_mean / block_time * 100;  // take audio processing time divide by block_time, convert to %

  if(processor_load >= 100.0) {
    processor_load = 100.0;
    valueColor = RA8875_RED;
  }

  tft.setFontScale((enum RA8875tsize)0);
  tft.setTextColor(valueColor);
  MyDrawFloatP(processor_load, 0, col, row, buff, 2);
  tft.print("%");
  elapsed_micros_idx_t = 0;
  elapsed_micros_sum = 0;
}

/*****
  Purpose: Information box follow up function for the FT8 item

  Parameter list:
    int row, col  Row and column of info box item
*****/
void IBFT8Followup(int row, int col) {
}

void IBFT8RxTxFollowup(int row, int col) {
  // update item changed
  tft.setFontScale((enum RA8875tsize)0);
  tft.setTextColor(RA8875_GREEN);
  tft.setCursor(col, row);
  if(col == IB_COL_1_X) {
    // tx is in column 1 *** TODO: automate this to look up proper column ***
    tft.print(ft8TxFreq);
  } else {
    tft.print(ft8RxFreq);
  }

  if(txEqualsRx) {
    // update other item
    if(col == IB_COL_1_X) {
      // tx is in column 1
      // update rx
      ft8RxFreq = ft8TxFreq;
      tft.fillRect(IB_COL_2_X, row, tft.getFontWidth() * 5, tft.getFontHeight(), RA8875_BLACK);
      tft.setCursor(IB_COL_2_X, row);
      tft.print(ft8RxFreq);
    } else {
      // update tx
      ft8TxFreq = ft8RxFreq;
      tft.fillRect(IB_COL_1_X, row, tft.getFontWidth() * 5, tft.getFontHeight(), RA8875_BLACK);
      tft.setCursor(IB_COL_1_X, row);
      tft.print(ft8RxFreq);
    }

    // draw TX = RX indicator
    tft.setCursor((IB_COL_1_X+IB_COL_2_X)/2-tft.getFontWidth()*3, row);
    tft.print("=");
  } else {
    // clear TX = RX indicator
    tft.setCursor((IB_COL_1_X+IB_COL_2_X)/2-tft.getFontWidth()*3, row);
    tft.print(" ");
  }
}

void ClearInfoBoxKeyer() {
  int row = infoBox[IB_ITEM_KEYER].row;

  ClearInfoBoxRow(row);
  ClearInfoBoxRow(row + 20);
  ClearInfoBoxRow(row + 40);
  DrawInfoBoxFrame();
}

/*****
  Purpose: Information box follow up function for the Keyer item

  Parameter list:
    int row, col  Row and column of info box item
*****/
void IBKeyerFollowup(int row, int col) {
  if(keyerState == 1) {
    tft.print(" ");
    tft.print(currentWPM);

    if(keyerMessagesActive) {
      if(keyerMessageEditMode) {
        tft.setTextColor(YELLOW);
      } else {
        tft.setTextColor(RA8875_GREEN);
      }
    } else {
      tft.setTextColor(RA8875_WHITE);
    }

    // show messages
    //                  1         2         3
    //         123456789012345678901234567890123
    //tft.print("123456789012345678901234567890123");
    //tft.print("                                 "); // doesn't clear line
    tft.fillRect(INFO_BOX_L + 5, row + 20, tft.getFontWidth() * 33, tft.getFontHeight(), RA8875_BLACK);
    tft.setCursor(INFO_BOX_L + 5, row + 20);
    tft.print(keyerMessages[selectedMsg]);

    if(keyerMessagesActive) {
      tft.setTextColor(RA8875_WHITE);
    } else {
      tft.setTextColor(RA8875_GREEN);
    }

    tft.fillRect(INFO_BOX_L + 5, row + 40, tft.getFontWidth() * 33, tft.getFontHeight(), RA8875_BLACK);
    tft.setCursor(INFO_BOX_L + 5, row + 40);
    msgBuffer[msgIndexIn] = 0; // terminate buffer
    tft.print((char *)msgBuffer);
  } else {
    // clear message lines
    tft.fillRect(INFO_BOX_L + 5, row + 20, tft.getFontWidth() * 33, tft.getFontHeight(), RA8875_BLACK);
    tft.fillRect(INFO_BOX_L + 5, row + 40, tft.getFontWidth() * 33, tft.getFontHeight(), RA8875_BLACK);
  }
}

/*****
  Purpose: Show estimated WPM in information box
           Assumes decoder is in column 1 row 9
*****/
void UpdateIBWPM() {
  int yOffset = infoBox[IB_ITEM_DECODER].row;

  tft.setFontScale((enum RA8875tsize)0);
  tft.setTextColor(RA8875_GREEN);
  tft.fillRect(IB_COL_1_X + 37, yOffset, tft.getFontWidth() * 10, tft.getFontHeight(), RA8875_BLACK);
  tft.setCursor(IB_COL_1_X + 38, yOffset);
  tft.print("(");
  tft.print(1200L / (dahLength / 3));
  tft.print(" WPM)");
}

/*****
  Purpose: Update CW decode lock indicator in information box
*****/
void UpdateDecodeLockIndicator() {
  int yOffset = infoBox[IB_ITEM_DECODER].row;

  // ==========  CW decode "lock" indicator
  if(combinedCoeff > 50)
  {
    tft.fillRect(IB_COL_2_X - 20, yOffset, 15, 15, RA8875_GREEN);
  }
  else if(combinedCoeff < 50)
  {
    CWLevelTimer = millis();
    if(CWLevelTimer - CWLevelTimerOld > 2000)
    {
      CWLevelTimerOld = millis();
      tft.fillRect(IB_COL_2_X - 20, yOffset, 17, 17, RA8875_BLACK);
    }
  }
}

void DrawInfoBoxFrame() {
  tft.drawRect(INFO_BOX_L, INFO_BOX_T, INFO_BOX_W, INFO_BOX_H, RA8875_LIGHT_GREY); // draw info box
}

void ClearInfoBoxContents() {
  tft.fillRect(INFO_BOX_L + 2, INFO_BOX_T + 2, INFO_BOX_W - 4, INFO_BOX_H - 5, RA8875_BLACK); // clear info box contents
}

/*****
  Purpose: This function draws the Info Box frame and clears the region within it
*****/
void ClearInfoBox() {
  ClearInfoBoxContents();
  DrawInfoBoxFrame();
}

/*****
  Purpose: Information box follow up function for the Stack item
            The stack value is more informative when called from within a function that might be stressing the stack

  Parameter list:
    int row, col  Row and column of info box item
*****/
void IBStackFollowup(int row, int col) {
  // note: these values are defined by the linker, they are not valid memory
  // locations in all cases - by defining them as arrays, the C++ compiler
  // will use the address of these definitions - it's a big hack, but there's
  // really no clean way to get at linker-defined symbols from the .ld file

  extern char _ebss[];

  auto sp = (char*) __builtin_frame_address(0);

  auto stack = (sp - _ebss) >> 10;

  //Serial.print("Stack: ");
  //Serial.println((int)(sp - _ebss));
  //Serial.println("");

  tft.setFontScale((enum RA8875tsize)0);
  tft.setTextColor(RA8875_GREEN);
  tft.setCursor(col, row);
  tft.print(stack);
  tft.print("k");
}


/*****
  Purpose: Information box follow up function for the Heap item
           Note: mallinfo() must be primed by fully allocating the
           heap at startup.  See PrimeMallInfo() in Utility.cpp.

  Parameter list:
    int row, col  Row and column of info box item
*****/
void IBHeapFollowup(int row, int col) {
  // note: these values are defined by the linker, they are not valid memory
  // locations in all cases - by defining them as arrays, the C++ compiler
  // will use the address of these definitions - it's a big hack, but there's
  // really no clean way to get at linker-defined symbols from the .ld file

  //extern char _heap_end[], *__brkval; // this is only useful at startup

  //struct mallinfo mi = mallinfo();

  //Serial.println(mi.arena);
  //Serial.println(mi.ordblks);
  //Serial.println(mi.smblks);
  //Serial.println(mi.hblks);
  //Serial.println(mi.hblkhd);
  //Serial.println(mi.usmblks);
  //Serial.println(mi.fsmblks);
  //Serial.println(mi.uordblks);
  //Serial.println(mi.fordblks);
  //Serial.println(mi.keepcost);

  size_t heap = mallinfo().fordblks;

  //Serial.println(mallinfo().fordblks);
  //Serial.println(heap);

  heap = mallinfo().fordblks >> 10;

  tft.setFontScale((enum RA8875tsize)0);
  tft.setTextColor(RA8875_GREEN);
  tft.setCursor(col, row);
  tft.print(heap);
  tft.print("k");
}

void SetFtActive(int flag) {
  if(flag == 1) {
    mouseCenterTuneActive = false;
    HighlightIBItem(IB_ITEM_FINE, RA8875_GREEN);
    HighlightIBItem(IB_ITEM_TUNE, RA8875_WHITE);
  } else {
    mouseCenterTuneActive = true;
    HighlightIBItem(IB_ITEM_TUNE, RA8875_GREEN);
    HighlightIBItem(IB_ITEM_FINE, RA8875_WHITE);
  }
}

// mouse actions
void MouseButtonInfoBox(int button, int x, int y) {
  // *** TODO: this is weak ***
  int item, itemX, itemY, itemSize, itemChars, itemW, itemH;

  //Serial.println(x);
  //Serial.println(y);
  //Serial.println(itemX);
  //Serial.println(itemY);
  //Serial.println(itemSize);
  //Serial.println(itemChars);
  //Serial.println(itemW);
  //Serial.println(itemH);


  // *** TODO: rework this after we add full capability
  for(int i = 0; i < 5; i++) {
    switch(i) {
      case 0:
        item = IB_ITEM_TUNE;
        break;
      case 1:
        item = IB_ITEM_FINE;
        break;
      case 2:
        item = IB_ITEM_ZOOM;
        break;
      case 3:
        item = IB_ITEM_FLOOR;
        break;
      case 4:
        item = IB_ITEM_DECODER;
        break;
    }

    itemX = infoBox[item].col;
    itemY = infoBox[item].row;
    itemSize = infoBox[item].fontSize;
    itemChars = infoBox[item].clearWidth;
    itemW = (itemSize == 1 ? 16 : 8) * itemChars;
    itemH = itemSize == 1 ? 32 : 16;

    // allow action within a portion of label as well
    if(x > itemX - 50 && x < itemX + itemW && y > itemY && y < itemY + itemH) {
      switch(item) {
        case IB_ITEM_TUNE:
          if(button == 1) {
            SetFtActive(0);
          }
          break;

        case IB_ITEM_FINE:
          if(button == 1) {
            SetFtActive(1);
          }
          break;

        case IB_ITEM_ZOOM:
          if(button == 1) {
            SetZoom(++spectrumZoom);
          } else {
            SetZoom(--spectrumZoom);
          }
          break;

        case IB_ITEM_FLOOR:
          ToggleLiveNoiseFloorFlag();
          break;

        case IB_ITEM_DECODER:
          ToggleCWDecoder();
          break;

        default:
          break;
      }
    }
  }
}

void MouseWheelInfoBox(int wheel, int x, int y) {
  int item, itemX, itemY, itemSize, itemChars, itemW, itemH;

  // *** TODO: this is weak ***
  for(int i = 0; i < 9; i++) {
    switch(i) {
      case 0:
        item = IB_ITEM_VOL;
        break;
      case 1:
        item = IB_ITEM_TUNE;
        break;
      case 2:
        item = IB_ITEM_FINE;
        break;
      case 3:
        item = IB_ITEM_ZOOM;
        break;
      case 4:
        item = IB_ITEM_FT8_TX;
        break;
      case 5:
        item = IB_ITEM_FT8_TXF;
        break;
      case 6:
        item = IB_ITEM_FT8_RXF;
        break;
      case 7:
        item = IB_ITEM_FT8_INT;
        break;
      case 8:
        item = IB_ITEM_FT8_CQ;
        break;
    }

    itemX = infoBox[item].col;
    itemY = infoBox[item].row;
    itemSize = infoBox[item].fontSize;
    itemChars = infoBox[item].clearWidth;
    itemW = (itemSize == 1 ? 16 : 8) * itemChars;
    itemH = itemSize == 1 ? 32 : 16;

    // allow action within a portion of label as well
    if(x > itemX - 50 && x < itemX + itemW && y > itemY && y < itemY + itemH) {
      //if(infoBox[item].option != NULL)
      //  Serial.print("before: "); Serial.println(*(infoBox[item].option));

      switch(item) {
        case IB_ITEM_VOL:
          t41.AudioVolume += wheel;
          break;

          case IB_ITEM_TUNE:
          ChangeFreqIncrement(wheel);
          if(mouseCenterTuneActive) {
            HighlightIBItem(IB_ITEM_TUNE, RA8875_GREEN);
          }
          break;

        case IB_ITEM_FINE:
          ChangeFtIncrement(wheel);
          if(!mouseCenterTuneActive) {
            HighlightIBItem(IB_ITEM_FINE, RA8875_GREEN);
          }
          break;

      case IB_ITEM_ZOOM:
          if(wheel == 1) {
            SetZoom(++spectrumZoom);
          } else {
            SetZoom(--spectrumZoom);
          }
          break;

      case IB_ITEM_FT8_TX:
        ChangeFt8TxState(wheel);
        break;
      case IB_ITEM_FT8_TXF:
        ChangeFt8TxFreq(wheel);
        break;
      case IB_ITEM_FT8_RXF:
        ChangeFt8RxFreq(wheel);
        break;
      case IB_ITEM_FT8_INT:
        ChangeFt8TxInterval(wheel);
        break;
      case IB_ITEM_FT8_CQ:
        ChangeFt8CqState(wheel);
        break;
        default:
          break;
      }

      //if(infoBox[item].option != NULL)
      //  Serial.print("after: "); Serial.println(*(infoBox[item].option));
      //Serial.print("item: "); Serial.println(item);
      //Serial.print("wheel: "); Serial.println(wheel);
      //Serial.print("x: "); Serial.println(x);
      //Serial.print("y: "); Serial.println(y);
      //Serial.print("itemX: "); Serial.println(itemX);
      //Serial.print("itemY: "); Serial.println(itemY);
      //Serial.print("itemSize: "); Serial.println(itemSize);
      //Serial.print("itemChars: "); Serial.println(itemChars);
      //Serial.print("itemW: "); Serial.println(itemW);
      //Serial.print("itemH: "); Serial.println(itemH);
      //Serial.println();

      break;
    }
  }
}

void HighlightIBItem(uint8_t item, int color) {
  int label_x;
  int xOffset = infoBox[item].col;
  int yOffset = infoBox[item].row;

  if(item >= IB_NUM_ITEMS) return;

  tft.setFontScale((enum RA8875tsize)infoBox[item].fontSize);
  //tft.fillRect(xOffset, yOffset, tft.getFontWidth() * infoBox[item].clearWidth, tft.getFontHeight(), RA8875_BLACK);
  tft.setTextColor(color);
  label_x = xOffset - 5 - strlen(infoBox[item].label) * tft.getFontWidth();
  tft.setCursor(label_x, yOffset);
  tft.print(infoBox[item].label);
}

/*****
  Purpose: DisplayClock()*****/
void DisplayClock() {
  char timeBuffer[15];
  char temp[5];

  temp[0]       = '\0';
  timeBuffer[0] = '\0';
  strcpy(timeBuffer, MY_TIMEZONE);         // e.g., EST
#ifdef TIME_24H
  itoa(hour(), temp, DEC);
#else
  itoa(hourFormat12(), temp, DEC);
#endif
  if(strlen(temp) < 2) {
    strcat(timeBuffer, "0");
  }
  strcat(timeBuffer, temp);
  strcat(timeBuffer, ":");

  itoa(minute(), temp, DEC);
  if(strlen(temp) < 2) {
    strcat(timeBuffer, "0");
  }
  strcat(timeBuffer, temp);
  strcat(timeBuffer, ":");

  itoa(second(), temp, DEC);
  if(strlen(temp) < 2) {
    strcat(timeBuffer, "0");
  }
  strcat(timeBuffer, temp);

  tft.setFontScale((enum RA8875tsize) 0);

  tft.fillRect(TIME_X, TIME_Y, 15 * tft.getFontWidth(), tft.getFontHeight(), RA8875_BLACK);
  tft.setCursor(TIME_X, TIME_Y);
  //tft.setTextColor(RA8875_WHITE);
  tft.setTextColor(YELLOW);
  tft.print(timeBuffer);
}

void UpdateClock() {
  // update clock
  if(ms_500.check() == 1) {
    DisplayClock();
  }
}
