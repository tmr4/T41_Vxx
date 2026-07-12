
#include <Audio.h>
#include <malloc.h>
#include <Metro.h>
#include <TimeLib.h>                   // Part of Teensy Time library

#include "..\..\SDT.h"

#include "..\..\AudioConfig.h"
#include "..\..\Button.h"
#include "..\..\ButtonProc.h"
#include "..\..\CWProcessing.h"
#include "Display.h"
#include "..\..\Display.h"
#include "..\..\DSP_Fn.h"
#include "..\..\Encoders.h"
#include "..\..\FIR.h"
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
void IBAGCFollowup(int row, int col);
void IBZoomFollowup(int row, int col);

void ClearInfoBox();
void ClearInfoBoxRow(int row);

//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

extern long loopTimeSum;
extern long loopCount;
extern int paddleFlip;
extern int currentMicThreshold;

Metro ms_500 = Metro(500); // display clock updates

typedef  struct {
  const int index;              // T41 element index
  const char *label;            // info box label
  const char** options;         // label options
  const int* option;            // pointer to option selector or pointer to the actual value if options is NULL
  const int fontSize;           // 0 - small or 1 - large font (large font takes two rows, adjust item rows and/or IB_ROW_#_Y accordingly)
  const int clearWidth;         // maximum number of characters to clear when updating field
  const int highlightFlag;      // 0 - highlight all options in green, 1 - don't highlight first option, 2 - first option white, second option red, other options green
  const int col, row;           // item column and row
  void (* const followFnPtr)(int row, int col);  // function to run after info box field is updated (note that these may be hard-coded to a particular location
} InfoBoxItem;

#define IB_COL_1_X        INFO_BOX_L + 90  // X coordinate for info box 1st column field
//#define IB_COL_2_X        INFO_BOX_L + 220 // X coordinate for info box 2nd column field
#define IB_COL_2_X        INFO_BOX_L + 230 // X coordinate for info box 2nd column field
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

const char* agcOpts[] = { "Off", "L", "S", "M", "F" };
const char* tuneValues[] = { "10", "50", "100", "250", "1000", "10000", "100000", "1000000" };
const char* ftValues[] = { "10", "50", "250", "500" };
const char* filter[] = { "Off", "Kim", "Spectral", "LMS" };
const char* onOff[2] = { "Off", "On" };
const char* nfOptions[3] = { "Off", "Auto", "On"};
const char* zoomOptions[] = { "1x ", "2x ", "4x ", "8x ", "16x" };
const char* keyerOpts[] = { "Off", "WPM" };
const char* optionsWPM[2] = { "Straight Key", "Paddles " };
const char* ft8Opts[] = { "no sync", "sync" };
const char* ft8TxOpts[] = { "Off", "enabled" };
const char* ft8IntOpts[] = { "even", "odd" };
const char* ft8CqOpts[] = { "man", "auto" };

#define IB_NUM_ITEMS 24

// active window info box item index into infoBox[]
//int infoBoxItemIndex[IB_NUM_ITEMS] = { -1 }; // this is sufficient, but comments below can be helpful here
int infoBoxItemIndex[IB_NUM_ITEMS] = {
   0,    // Vol
   1,    // AGC
   2,    // CT Inc
   3,    // FT Inc
   4,    // Zoom
   5,    // Noise Floor
   6,    // Notch
   7,    // Noise Filter
   8,    // Compress
   9,    // RF Gain
  10,    // Equalizers
  -1,    // Decoder
  -1,    // Key Type
  -1,    // Keyer
  -1,    // FT8 sync
  -1,    // FT8 Tx interval
  -1,    // FT8 Tx enabled
  -1,    // FT8 Tx interval
  -1,    // FT8 Tx freq
  -1,    // FT8 Rx freq
  20,    // Stack
  21,    // Heap
  22,    // Teensy Temp
  23     // Teensy Load
};

//                                                                                                                            font    # chars
//                                             index            label          options      option                            size    to erase  flag  col            row,           follow-up function
PROGMEM const InfoBoxItem volInfoBoxItem   = { T41_ITEM_VOL,    "Vol:",        NULL,        (int*)&t41.AudioVolume.value,      1,        3,      0,   IB_COL_1_X,    IB_ROW_1_Y,    &IBVolFollowup         }; // Vol
PROGMEM const InfoBoxItem agcInfoBoxItem   = { T41_ITEM_AGC,    "AGC",         agcOpts,     (int*)&t41.AGCMode.value,          1,        3,      1,   IB_COL_2L_X,   IB_ROW_1_Y,    &IBAGCFollowup         }; // AGC
PROGMEM const InfoBoxItem ctInfoBoxItem    = { T41_ITEM_TUNE,   "CT Inc:",     tuneValues,  (int*)&t41.CenterTuneIndex.value,  0,        7,      0,   IB_COL_1_X,    IB_ROW_3_Y,    &IBTuneIncFollowup     }; // CT Inc
PROGMEM const InfoBoxItem ftInfoBoxItem    = { T41_ITEM_FINE,   "FT Inc:",     ftValues,    (int*)&t41.FineTuneIndex.value,    0,        3,      0,   IB_COL_2_X,    IB_ROW_3_Y,    &IBTuneIncFollowup     }; // FT Inc
PROGMEM const InfoBoxItem zoomInfoBoxItem  = { T41_ITEM_ZOOM,   "Zoom:",       zoomOptions, (int*)&t41.SpectrumZoom.value,     0,        3,      0,   IB_COL_1_X,    IB_ROW_4_Y,    &IBZoomFollowup        }; // Zoom
PROGMEM const InfoBoxItem nfInfoBoxItem    = { T41_ITEM_FLOOR,  "NF Set:",     nfOptions,   (int*)&t41.LiveNoiseFloor.value,   0,        4,      1,   IB_COL_2_X,    IB_ROW_4_Y,    NULL                   }; // Noise Floor
PROGMEM const InfoBoxItem notchInfoBoxItem = { T41_ITEM_NOTCH,  "Notch:",      onOff,       (int*)&ANR_notchOn,                0,        3,      1,   IB_COL_2_X,    IB_ROW_5_Y,    NULL                   }; // Notch

// Compress and Noise need to be in column 1
PROGMEM const InfoBoxItem noiseInfoBoxItem    = { T41_ITEM_FILTER,   "Noise:",      filter,      (int*)&t41.NoiseFilter.value,      0,        8,      1,   IB_COL_1_X,    IB_ROW_5_Y,    NULL                   }; // Noise Filter
PROGMEM const InfoBoxItem compressInfoBoxItem = { T41_ITEM_COMPRESS, "Compress:",   onOff,       (int*)&t41.Compressor.value,       0,        6,      1,   IB_COL_1_X,    IB_ROW_6_Y,    &IBCompressionFollowup }; // Compress
PROGMEM const InfoBoxItem rfgainInfoBoxItem   = { T41_ITEM_RFGAIN,   "RF Gain:",    NULL,        NULL,                              0,        3,      0,   IB_COL_2_X,    IB_ROW_6_Y,    &IBRFGainFollowup      }; // RF Gain

// Equalizers takes two columns
PROGMEM const InfoBoxItem equalizerInfoBoxItem   = { T41_ITEM_EQUALIZER, "Equalizer:",  NULL,        NULL,                              0,       10,      1,   IB_COL_1_X,    IB_ROW_7_Y,    &IBEQFollowup          }; // Equalizers

// Decoder takes two columns
PROGMEM const InfoBoxItem decoderInfoBoxItem   = { T41_ITEM_DECODER, "Decoder:",    onOff,       (int*)&t41.CWDecoder.value,        0,        3,      1,   IB_COL_1_X,    IB_ROW_8_Y,    NULL                   }; // Decoder

// Key type takes two columns
PROGMEM const InfoBoxItem keytypeInfoBoxItem   = { T41_ITEM_KEY, "Key Type:",   optionsWPM,  (int*)&t41.KeyType.value,          0,        2,      0,   IB_COL_1_X,    IB_ROW_9_Y,    &IBWPMFollowup         }; // Key Type

// Memory keyer requires 3 rows (10-12)
PROGMEM const InfoBoxItem keyerInfoBoxItem   = { T41_ITEM_KEYER, "Keyer     ",  keyerOpts,   &keyerState,                 0,       10,      1,   IB_COL_1_X,    IB_ROW_10_Y,    &IBKeyerFollowup      }; // Keyer

// FT8 items
PROGMEM const InfoBoxItem ft8InfoBoxItem        = { T41_ITEM_FT8,      "FT8       ",  ft8Opts,     &ft8SyncState,               0,        8,      1,   IB_COL_1_X,    IB_ROW_9_Y,    NULL                   }; // FT8 sync
PROGMEM const InfoBoxItem ft8TxIntInfoBoxItem   = { T41_ITEM_FT8_INT,  "Tx Int:",     ft8IntOpts,  &ft8IntState,                0,        4,      0,   IB_COL_2_X,    IB_ROW_9_Y,    NULL                   }; // FT8 Tx interval
PROGMEM const InfoBoxItem ft8TxInfoBoxItem      = { T41_ITEM_FT8_TX,   "Tx:",         ft8TxOpts,   &ft8TxState,                 0,        7,      1,   IB_COL_1_X,    IB_ROW_10_Y,   NULL                   }; // FT8 Tx enabled
PROGMEM const InfoBoxItem ft8CQInfoBoxItem      = { T41_ITEM_FT8_CQ,   "CQ resp:",    ft8CqOpts,   &ft8CqState,                 0,        4,      1,   IB_COL_2_X,    IB_ROW_10_Y,   NULL                   }; // FT8 Tx interval
PROGMEM const InfoBoxItem ft8TxFreqInfoBoxItem  = { T41_ITEM_FT8_TXF,  "Tx Freq:",    NULL,        &ft8TxFreq,                  0,        5,      0,   IB_COL_1_X,    IB_ROW_11_Y,   &IBFT8RxTxFollowup     }; // FT8 Tx freq
PROGMEM const InfoBoxItem ft8RxFreqInfoBoxItem  = { T41_ITEM_FT8_RXF,  "Rx Freq:",    NULL,        &ft8RxFreq,                  0,        5,      0,   IB_COL_2_X,    IB_ROW_11_Y,   &IBFT8RxTxFollowup     }; // FT8 Rx freq

PROGMEM const InfoBoxItem stackInfoBoxItem   = { T41_ITEM_STACK, "Stack:",      NULL,        NULL,                        0,        4,      2,   IB_COL_2_X,    IB_ROW_13_Y,   &IBStackFollowup       }; // Stack

// Heap should be in column 1
PROGMEM const InfoBoxItem heapInfoBoxItem   = { T41_ITEM_HEAP, "Heap:",       NULL,        NULL,                        0,        10,     2,   IB_COL_1_X,    IB_ROW_13_Y,   &IBHeapFollowup        }; // Heap
PROGMEM const InfoBoxItem tempInfoBoxItem   = { T41_ITEM_TEMP, "Temp:",       NULL,        NULL,                        0,        3,      1,   IB_COL_2_X,    IB_ROW_14_Y,   &IBTempFollowup        }; // Teensy Temp

// Load should be in column 1
PROGMEM const InfoBoxItem loadInfoBoxItem   = { T41_ITEM_LOAD, "Load:",       NULL,        NULL,                        0,        8,      1,   IB_COL_1_X,    IB_ROW_14_Y,   &IBLoadFollowup        };  // Teensy Load

PROGMEM const InfoBoxItem infoBox[IB_NUM_ITEMS] = {
  volInfoBoxItem,       // Vol
  agcInfoBoxItem,       // AGC
  ctInfoBoxItem,        // CT Inc
  ftInfoBoxItem,        // FT Inc
  zoomInfoBoxItem,      // Zoom
  nfInfoBoxItem,        // Noise Floor
  notchInfoBoxItem,     // Notch
  noiseInfoBoxItem,     // Noise Filter
  compressInfoBoxItem,  // Compress
  rfgainInfoBoxItem,    // RF Gain
  equalizerInfoBoxItem, // Equalizers
  decoderInfoBoxItem,   // Decoder
  keytypeInfoBoxItem,   // Key Type
  keyerInfoBoxItem,     // Keyer
  ft8InfoBoxItem,       // FT8 sync
  ft8TxIntInfoBoxItem,  // FT8 Tx interval
  ft8TxInfoBoxItem,     // FT8 Tx enabled
  ft8CQInfoBoxItem,     // FT8 Tx interval
  ft8TxFreqInfoBoxItem, // FT8 Tx freq
  ft8RxFreqInfoBoxItem, // FT8 Rx freq
  stackInfoBoxItem,     // Stack
  heapInfoBoxItem,      // Heap
  tempInfoBoxItem,      // Teensy Temp
  loadInfoBoxItem       // Teensy Load
};

#define GENERIC_IB_NUM_ITEMS 15
PROGMEM const InfoBoxItem genericInfoBox[GENERIC_IB_NUM_ITEMS] = {
  volInfoBoxItem,       // Vol
  agcInfoBoxItem,       // AGC
  ctInfoBoxItem,        // CT Inc
  ftInfoBoxItem,        // FT Inc
  zoomInfoBoxItem,      // Zoom
  nfInfoBoxItem,        // Noise Floor
  notchInfoBoxItem,     // Notch
  noiseInfoBoxItem,     // Noise Filter
  compressInfoBoxItem,  // Compress
  rfgainInfoBoxItem,    // RF Gain
  equalizerInfoBoxItem, // Equalizers
  stackInfoBoxItem,     // Stack
  heapInfoBoxItem,      // Heap
  tempInfoBoxItem,      // Teensy Temp
  loadInfoBoxItem       // Teensy Load
};

#define CW_IB_NUM_ITEMS 18
PROGMEM const InfoBoxItem cwInfoBox[CW_IB_NUM_ITEMS] = {
  volInfoBoxItem,       // Vol
  agcInfoBoxItem,       // AGC
  ctInfoBoxItem,        // CT Inc
  ftInfoBoxItem,        // FT Inc
  zoomInfoBoxItem,      // Zoom
  nfInfoBoxItem,        // Noise Floor
  notchInfoBoxItem,     // Notch
  noiseInfoBoxItem,     // Noise Filter
  compressInfoBoxItem,  // Compress
  rfgainInfoBoxItem,    // RF Gain
  equalizerInfoBoxItem, // Equalizers
  decoderInfoBoxItem,   // Decoder
  keytypeInfoBoxItem,   // Key Type
  keyerInfoBoxItem,     // Keyer
  stackInfoBoxItem,     // Stack
  heapInfoBoxItem,      // Heap
  tempInfoBoxItem,      // Teensy Temp
  loadInfoBoxItem       // Teensy Load
};

#define FT8_IB_NUM_ITEMS 20
PROGMEM const InfoBoxItem ft8InfoBox[FT8_IB_NUM_ITEMS] = {
  volInfoBoxItem,       // Vol
  agcInfoBoxItem,       // AGC
  ctInfoBoxItem,        // CT Inc
  ftInfoBoxItem,        // FT Inc
  zoomInfoBoxItem,      // Zoom
  nfInfoBoxItem,        // Noise Floor
  notchInfoBoxItem,     // Notch
  noiseInfoBoxItem,     // Noise Filter
  compressInfoBoxItem,  // Compress
  rfgainInfoBoxItem,    // RF Gain
  ft8InfoBoxItem,       // FT8 sync
  ft8TxIntInfoBoxItem,  // FT8 Tx interval
  ft8TxInfoBoxItem,     // FT8 Tx enabled
  ft8CQInfoBoxItem,     // FT8 Tx interval
  ft8TxFreqInfoBoxItem, // FT8 Tx freq
  ft8RxFreqInfoBoxItem, // FT8 Rx freq
  stackInfoBoxItem,     // Stack
  heapInfoBoxItem,      // Heap
  tempInfoBoxItem,      // Teensy Temp
  loadInfoBoxItem       // Teensy Load
};

#define CAL_IB_NUM_ITEMS 10
PROGMEM const InfoBoxItem calInfoBox[CAL_IB_NUM_ITEMS] = {
  volInfoBoxItem,       // Vol
  agcInfoBoxItem,       // AGC
  ctInfoBoxItem,        // CT Inc
  ftInfoBoxItem,        // FT Inc
  zoomInfoBoxItem,      // Zoom
  nfInfoBoxItem,        // Noise Floor
  stackInfoBoxItem,     // Stack
  heapInfoBoxItem,      // Heap
  tempInfoBoxItem,      // Teensy Temp
  loadInfoBoxItem       // Teensy Load
};

const InfoBoxItem* activeInfoBox = genericInfoBox;

//-------------------------------------------------------------------------------------------------------------
// Code
//-------------------------------------------------------------------------------------------------------------

void SetInfoBoxWindow(int window) {
  const InfoBoxItem* ib = infoBox;
  int numItems = IB_NUM_ITEMS;

  switch(window) {
    case 0: // most radio modes
      ib = genericInfoBox;
      numItems = GENERIC_IB_NUM_ITEMS;
      break;
    case 1: // CW
      ib = cwInfoBox;
      numItems = CW_IB_NUM_ITEMS;
      break;
    case 2: // internal FT8
      ib = ft8InfoBox;
      numItems = FT8_IB_NUM_ITEMS;
      break;
    case 3: // calibration
      ib = calInfoBox;
      numItems = CAL_IB_NUM_ITEMS;
      break;
  }

  // note which items are active in this window
  for(int i = 0; i < IB_NUM_ITEMS; i++) {
    infoBoxItemIndex[i] = -1;
    for(int j = 0; j < numItems; j++) {
      if(ib[j].index == infoBox[i].index) {
        infoBoxItemIndex[i] = j;
        break;
      }
      //Serial.println(infoBoxItemIndex[i]);
    }
  }
  activeInfoBox = ib;
  UpdateInfoBox();
}

/*****
  Update the specified information box item

  Parameter list:
    int index   index of the info box item to update
*****/
void UpdateInfoBoxItem(int index) {
  int item, label_x, xOffset, yOffset;

  if(index >= IB_NUM_ITEMS || index < 0) return;
  item = infoBoxItemIndex[index];
  if(item == -1) return;

  xOffset = activeInfoBox[item].col;
  yOffset = activeInfoBox[item].row;
  tft.setFontScale((enum RA8875tsize)activeInfoBox[item].fontSize);
  tft.fillRect(xOffset, yOffset, tft.getFontWidth() * activeInfoBox[item].clearWidth, tft.getFontHeight(), RA8875_BLACK);
  tft.setTextColor(RA8875_WHITE);
  label_x = xOffset - 5 - strlen(activeInfoBox[item].label) * tft.getFontWidth();
  tft.setCursor(label_x, yOffset);
  tft.print(activeInfoBox[item].label);

  if(activeInfoBox[item].options != NULL) {
    if((activeInfoBox[item].highlightFlag > 0) && (*activeInfoBox[item].option == 0)) {
      tft.setTextColor(RA8875_WHITE);
    } else if((activeInfoBox[item].highlightFlag == 2) && (*activeInfoBox[item].option == 1)) {
      tft.setTextColor(RA8875_RED);
    } else {
      tft.setTextColor(RA8875_GREEN);
    }

    tft.setCursor(xOffset, yOffset);
    tft.print(activeInfoBox[item].options[*activeInfoBox[item].option]);
  }

  if(activeInfoBox[item].followFnPtr != NULL) {
    activeInfoBox[item].followFnPtr(activeInfoBox[item].row, activeInfoBox[item].col);
  }
}

void ShowVersion() {
  int xOffset = TIME_X + 18 * tft.getFontWidth();
  tft.setFontScale((enum RA8875tsize) 0);
  tft.fillRect(xOffset, TIME_Y, YPIXELS-xOffset-2, tft.getFontHeight(), RA8875_BLACK);
  tft.setCursor(xOffset, TIME_Y);
  tft.setTextColor(YELLOW);
  switch(t41.CalState) {
    case FREQ_CAL_STATE:
      tft.print("Freq Cal");
      break;

    case RXIQ_CAL_STATE:
      break;

    case TXIQ_CAL_STATE:
      break;

    default:
      tft.print(VERSION);
      break;
  }
}

/*****
  Purpose: Updates the information box
*****/
void UpdateInfoBox() {
  ClearInfoBox();
  ShowVersion();

  // you can update each item individually if they need done in a particular order ...
  //UpdateInfoBoxItem(T41_ITEM_VOL);
  //UpdateInfoBoxItem(T41_ITEM_AGC);
  //UpdateInfoBoxItem(T41_ITEM_TUNE);
  //UpdateInfoBoxItem(T41_ITEM_FINE);
  //UpdateInfoBoxItem(T41_ITEM_COMPRESS);
  //UpdateInfoBoxItem(T41_ITEM_DECODER);
  //UpdateInfoBoxItem(T41_ITEM_FILTER);
  //UpdateInfoBoxItem(T41_ITEM_FLOOR);
  //UpdateInfoBoxItem(T41_ITEM_NOTCH);
  //UpdateInfoBoxItem(T41_ITEM_KEY);
  //UpdateInfoBoxItem(T41_ITEM_ZOOM);

  // ... or update them in order
  for(int i = 0; i < IB_NUM_ITEMS; i++) {
    UpdateInfoBoxItem(i);
  }
}

void ClearInfoBoxRow(int row) {
  tft.fillRect(INFO_BOX_L + 2, row,  INFO_BOX_W - 6, 20, RA8875_BLACK);
}

void HighlightTuneInc() {
  if(t41.MouseCenterTuneActive) {
    HighlightIBItem(T41_ITEM_TUNE, RA8875_GREEN);
    HighlightIBItem(T41_ITEM_FINE, RA8875_WHITE);
  } else {
    HighlightIBItem(T41_ITEM_FINE, RA8875_GREEN);
    HighlightIBItem(T41_ITEM_TUNE, RA8875_WHITE);
  }
}

/*****
  Purpose: Information box follow up function for the tuning increment items

  Parameter list:
    int row, col  Row and column of info box item
*****/
void IBTuneIncFollowup(int row, int col) {
  HighlightTuneInc();
}

void IBZoomFollowup(int row, int col) {
  InitZoomFFTFilter(t41.SampleRate);
  UpdateDisplayZoom();
}

/*****
  Purpose: Information box follow up function for the Compression item
           Assumes this is only called as part of updating Compression item

  Parameter list:
    int row, col  Row and column of info box item
*****/
void IBCompressionFollowup(int row, int col) {
  if(t41.Compressor == 1) {
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
  if(t41.KeyType == 1) { // 1 = paddles
    if(paddleFlip == 0) {
      tft.print("R");
    } else {
      tft.print("L");
    }
    tft.print(" ");
    tft.print(t41.CurrentWPM);
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

void IBAGCFollowup(int row, int col) {
  AGCLoadValues();
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
  tft.print(t41.RFGain);
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
  float value = 0.0;
  int valueColor = RA8875_GREEN;
  static bool showFPS = false; // alternate between load % and fps
  int digits = showFPS ? 1 : 0;

  tft.setFontScale((enum RA8875tsize)0);
  if(showFPS) {
    // calc FPS
    if(loopTimeSum != 0) value = (float)loopCount / (float)loopTimeSum * 1000.0;
    // limit fps for infobox
    if(value >= 100) value = 99.9;
    if(value < 0) value = 0.0;
    loopTimeSum = 0;
    loopCount = 0;
  } else {
    // calc processor_load
    float block_time, mean = 0.0;

    if(elapsed_micros_idx_t != 0) mean = elapsed_micros_sum / elapsed_micros_idx_t;

    block_time = 128.0 / t41.SampleRate;  // one audio block is 128 samples and uses this in seconds
    block_time = block_time * 16;

    block_time *= 1000000.0;          // now in µseconds
    value = mean / block_time * 100;  // take audio processing time divide by block_time, convert to %

    if(value >= 100.0) {
      value = 100.0;
      valueColor = RA8875_RED;
    }

    elapsed_micros_idx_t = 0;
    elapsed_micros_sum = 0;
  }

  tft.setTextColor(valueColor);
  MyDrawFloatP(value, digits, col, row, buff, 2);
  if(showFPS) {
    tft.print(" fps");
  } else {
    tft.print("%");
  }

  showFPS = !showFPS; // show the other one next time
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
  int row = activeInfoBox[T41_ITEM_KEYER].row;

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
    tft.print(t41.CurrentWPM);

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
  int yOffset = activeInfoBox[T41_ITEM_DECODER].row;

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
  int yOffset = activeInfoBox[T41_ITEM_DECODER].row;

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

extern volatile uint32_t recentMaxStackUsage;
extern volatile uint32_t maxStackUsage;

/*****
  Purpose: Information box follow up function for the Stack item
    Provides three views of stack in successive calls:
      Total stack:  simply _estack - _ebss
      Recent Max:   maximum stack usage since last recent max
      Max:          maximum stack usage since program start

    note: _estack, _ebss are defined by the linker, they are not valid memory
    locations in all cases - by defining them as arrays, the C++ compiler
    will use the address of these definitions - it's a big hack, but there's
    really no clean way to get at linker-defined symbols from the .ld file
    *** TODO: is this comment still valid? ***

    *** The stack value is more informative when called from within
        a function that might be stressing the stack. ***

  Parameter list:
    int row, col  Row and column of info box item
*****/
void IBStackFollowup(int row, int col) {
  uint32_t value = _estack - _ebss;
  uint16_t color = RA8875_WHITE;
  static uint8_t showStack = 0; // 0=total stack, 1=recent max, 2=max

  noInterrupts();
  switch(showStack) {
    case 1:
      color = RA8875_GREEN;
      value = recentMaxStackUsage;
      recentMaxStackUsage = 0;
      break;
    case 2:
      if(maxStackUsage > value * 0.75) {
        color = RA8875_RED;
      } else {
        color = RA8875_YELLOW;
      }
      value = maxStackUsage;
      break;
  }
  interrupts();

  tft.setFontScale((enum RA8875tsize)0);
  tft.setCursor(col, row);
  tft.setTextColor(color);
  tft.print(value / 1000);
  tft.print("k");
  if(++showStack > 2) showStack = 0;
}

/*****
  Purpose: Information box follow up function for the Heap item
           Note: mallinfo() must be primed by fully allocating the
           heap at startup.  See PrimeMallInfo() in Utility.cpp.

  Parameter list:
    int row, col  Row and column of info box item
*****/
void IBHeapFollowup(int row, int col) {
  static bool showAudioBlocks = false; // alternate between heap and audio mem
  static int showAudioItem = 0; // 0=current usage, 1=max usage, 2=dropped block
  static uint32_t maxBlocks = 0;
  //size_t value = 0;
  uint32_t value = 0;
  int color = RA8875_GREEN;

  if(showAudioBlocks) {
    // skip drop block notice if there is none
    if(showAudioItem == 2 && !t41.DroppedBlock) showAudioItem = 0;
    switch(showAudioItem) {
      case 0:
        value = AudioMemoryUsage();
        break;
      case 1:
        value = AudioMemoryUsageMax();
        if(value > maxBlocks) {
          maxBlocks = value;
        } else {
          value = maxBlocks;
        }
        AudioMemoryUsageMaxReset(); // reset max audio mem usage for next loop
        break;
      case 2:
        if(t41.DroppedBlock) {
          color = RA8875_RED;
          t41.DroppedBlock = 0;
        }
        break;
    }
    // highlight high audio memory usage
    if(value > 0.75 * MAX_AUDIO_BLOCKS) color = RA8875_RED;
  } else {
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

    //value = mallinfo().fordblks;

    //Serial.println(mallinfo().fordblks);
    //Serial.println(heap);

    value = mallinfo().fordblks >> 10;
  }

  tft.setFontScale((enum RA8875tsize)0);
  tft.setTextColor(color);
  tft.setCursor(col, row);
  if(showAudioBlocks && showAudioItem == 2) {
    if(t41.DroppedBlock) tft.print("bk dropped");
  } else {
    tft.print(value);
    if(showAudioBlocks) {
      if(showAudioItem) {
        tft.print(" bk max");
      } else {
        tft.print(" bk cur");
      }
    } else {
      tft.print("k");
    }
  }
  showAudioBlocks = !showAudioBlocks; // show the other one next time
  if(showAudioBlocks) {
    // show next audio block item next time
    ++showAudioItem;
    if(showAudioItem > 2) showAudioItem = 0;
  }
}

// mouse actions
void MouseButtonInfoBox(int button, int x, int y) {
  // *** TODO: this is weak ***
  int index, item, itemX, itemY, itemSize, itemChars, itemW, itemH;

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
        index = T41_ITEM_TUNE;
        break;
      case 1:
        index = T41_ITEM_FINE;
        break;
      case 2:
        index = T41_ITEM_ZOOM;
        break;
      case 3:
        index = T41_ITEM_FLOOR;
        break;
      case 4:
        index = T41_ITEM_DECODER;
        break;
    }
    item = infoBoxItemIndex[index];
    if(item == -1) return;

    itemX = activeInfoBox[item].col;
    itemY = activeInfoBox[item].row;
    itemSize = activeInfoBox[item].fontSize;
    itemChars = activeInfoBox[item].clearWidth;
    itemW = (itemSize == 1 ? 16 : 8) * itemChars;
    itemH = itemSize == 1 ? 32 : 16;

    // allow action within a portion of label as well
    if(x > itemX - 50 && x < itemX + itemW && y > itemY && y < itemY + itemH) {
      switch(item) {
        case T41_ITEM_TUNE:
          if(button == 1) {
            t41.MouseCenterTuneActive = 1;
          }
          break;

        case T41_ITEM_FINE:
          if(button == 1) {
            t41.MouseCenterTuneActive = 0;
          }
          break;

        case T41_ITEM_ZOOM:
          if(button == 1) {
            t41.SpectrumZoom += 1;
          } else {
            t41.SpectrumZoom -= 1;
          }
          break;

        case T41_ITEM_FLOOR:
          t41.LiveNoiseFloor += 1;
          break;

        case T41_ITEM_DECODER:
          ToggleCWDecoder();
          break;

        default:
          break;
      }
    }
  }
}

void MouseWheelInfoBox(int wheel, int x, int y) {
  int index, item, itemX, itemY, itemSize, itemChars, itemW, itemH;

  // *** TODO: this is weak ***
  for(int i = 0; i < 10; i++) {
    switch(i) {
      case 0:
        index = T41_ITEM_VOL;
        break;
      case 1:
        index = T41_ITEM_AGC;
        break;
      case 2:
        index = T41_ITEM_TUNE;
        break;
      case 3:
        index = T41_ITEM_FINE;
        break;
      case 4:
        index = T41_ITEM_ZOOM;
        break;
      case 5:
        index = T41_ITEM_FT8_TX;
        break;
      case 6:
        index = T41_ITEM_FT8_TXF;
        break;
      case 7:
        index = T41_ITEM_FT8_RXF;
        break;
      case 8:
        index = T41_ITEM_FT8_INT;
        break;
      case 9:
        index = T41_ITEM_FT8_CQ;
        break;
    }
    item = infoBoxItemIndex[index];
    if(item == -1) return;

    itemX = activeInfoBox[item].col;
    itemY = activeInfoBox[item].row;
    itemSize = activeInfoBox[item].fontSize;
    itemChars = activeInfoBox[item].clearWidth;
    itemW = (itemSize == 1 ? 16 : 8) * itemChars;
    itemH = itemSize == 1 ? 32 : 16;

    // allow action within a portion of label as well
    if(x > itemX - 50 && x < itemX + itemW && y > itemY && y < itemY + itemH) {
      //if(activeInfoBox[item].option != NULL)
      //  Serial.print("before: "); Serial.println(*(activeInfoBox[item].option));

      switch(item) {
        case T41_ITEM_VOL:
          t41.AudioVolume += wheel;
          break;

        case T41_ITEM_AGC:
          t41.AGCMode += wheel;
          break;

          case T41_ITEM_TUNE:
          ChangeFreqIncrement(wheel);
          if(t41.MouseCenterTuneActive) {
            HighlightIBItem(T41_ITEM_TUNE, RA8875_GREEN);
          }
          break;

        case T41_ITEM_FINE:
          ChangeFtIncrement(wheel);
          if(!t41.MouseCenterTuneActive) {
            HighlightIBItem(T41_ITEM_FINE, RA8875_GREEN);
          }
          break;

      case T41_ITEM_ZOOM:
          if(wheel == 1) {
            t41.SpectrumZoom += 1;
          } else {
            t41.SpectrumZoom -= 1;
          }
          break;

      case T41_ITEM_FT8_TX:
        ChangeFt8TxState(wheel);
        break;
      case T41_ITEM_FT8_TXF:
        ChangeFt8TxFreq(wheel);
        break;
      case T41_ITEM_FT8_RXF:
        ChangeFt8RxFreq(wheel);
        break;
      case T41_ITEM_FT8_INT:
        ChangeFt8TxInterval(wheel);
        break;
      case T41_ITEM_FT8_CQ:
        ChangeFt8CqState(wheel);
        break;
        default:
          break;
      }

      //if(activeInfoBox[item].option != NULL)
      //  Serial.print("after: "); Serial.println(*(activeInfoBox[item].option));
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

void HighlightIBItem(uint8_t index, int color) {
  int item, label_x, xOffset, yOffset;

  if(index >= IB_NUM_ITEMS || index < 0) return;
  item = infoBoxItemIndex[index];
  if(item == -1) return;

  xOffset = activeInfoBox[item].col;
  yOffset = activeInfoBox[item].row;
  tft.setFontScale((enum RA8875tsize)activeInfoBox[item].fontSize);
  //tft.fillRect(xOffset, yOffset, tft.getFontWidth() * activeInfoBox[item].clearWidth, tft.getFontHeight(), RA8875_BLACK);
  tft.setTextColor(color);
  label_x = xOffset - 5 - strlen(activeInfoBox[item].label) * tft.getFontWidth();
  tft.setCursor(label_x, yOffset);
  tft.print(activeInfoBox[item].label);
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
