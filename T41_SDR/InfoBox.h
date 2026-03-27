
//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

#define IB_ITEM_VOL       0
#define IB_ITEM_AGC       1
#define IB_ITEM_TUNE      2
#define IB_ITEM_FINE      3
#define IB_ITEM_ZOOM      4
#define IB_ITEM_FLOOR     5
#define IB_ITEM_NOTCH     6
#define IB_ITEM_COMPRESS  7
#define IB_ITEM_FILTER    8
#define IB_ITEM_RFGAIN    9
#define IB_ITEM_EQUALIZER 10
#define IB_ITEM_DECODER   11
#define IB_ITEM_KEY       12
#define IB_ITEM_KEYER     13
#define IB_ITEM_FT8       14
#define IB_ITEM_FT8_INT   15
#define IB_ITEM_FT8_TX    16
#define IB_ITEM_FT8_CQ    17
#define IB_ITEM_FT8_TXF   18
#define IB_ITEM_FT8_RXF   19
#define IB_ITEM_STACK     20
#define IB_ITEM_HEAP      21
#define IB_ITEM_TEMP      22
#define IB_ITEM_LOAD      23

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

void DisplayClock();
void UpdateClock();
