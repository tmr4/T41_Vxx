
//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

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
