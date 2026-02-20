
//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

// info box coordinates and item identifiers
#define INFO_BOX_L        (SPECTRUM_LEFT_X + SPECTRUM_RES + 15)
#define INFO_BOX_T        (SPECTRUM_TOP_Y + SPECTRUM_HEIGHT + 40)
#define INFO_BOX_W        XPIXELS - INFO_BOX_L // use up remainder of screen right
#define INFO_BOX_H        YPIXELS - INFO_BOX_T // use up remainder of screen bottom

#define IB_ITEM_VOL       0
#define IB_ITEM_AGC       1
#define IB_ITEM_TUNE      2
#define IB_ITEM_FINE      3
#define IB_ITEM_ZOOM      4
#define IB_ITEM_DECODER   5
#define IB_ITEM_FLOOR     6
#define IB_ITEM_TEMP      7
#define IB_ITEM_LOAD      8
#define IB_ITEM_FT8       9
#define IB_ITEM_FT8_TX    10
#define IB_ITEM_FT8_TXF   11
#define IB_ITEM_FT8_RXF   12
#define IB_ITEM_FT8_INT   13
#define IB_ITEM_FT8_CQ    14
#define IB_ITEM_KEYER     15
#define IB_ITEM_STACK     16
#define IB_ITEM_HEAP      17
#define IB_ITEM_NOTCH     18
#define IB_ITEM_FILTER    19
#define IB_ITEM_COMPRESS  20
#define IB_ITEM_KEY       21
#define IB_ITEM_RFGAIN    22
#define IB_ITEM_EQUALIZER 23

extern bool infoBoxItemActive[];

//-------------------------------------------------------------------------------------------------------------
// Code
//-------------------------------------------------------------------------------------------------------------

void UpdateInfoBox();
void UpdateInfoBoxItem(uint8_t item);
void UpdateIBWPM();
void UpdateDecodeLockIndicator();

void DrawInfoBoxFrame();
void ClearInfoBox();

void MouseButtonInfoBox(int button, int cursorX, int cursorY);
void MouseWheelInfoBox(int wheel, int x, int y);
void HighlightIBItem(uint8_t item, int color);

void SetFtActive(int flag);

void ClearInfoBoxKeyer();
void ClearInfoBoxFT8();
