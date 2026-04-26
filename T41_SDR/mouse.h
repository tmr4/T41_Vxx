
/*

A T41 mouse driver must define the following display specific functions for normal operation.
If the mouse doesn't impliment a given functionality, the function can be empty.

Required display specific functions:

  GetCursorWidth
  GetCursorHeight

  CursorInMenuArea
  CursorInFreqArea
  CursorInOpStatsArea
  CursorInAudioSpectrum
  CursorInSpectrumWaterfall
  CursorInInfoBox
  MouseButtonFreqArea
  MouseWheelFreqAra
  MouseButtonOpStatsArea

*/

//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

extern int mouseWheelValue;
extern int menuBarSelected;

//-------------------------------------------------------------------------------------------------------------
// Code
//-------------------------------------------------------------------------------------------------------------

void MouseInit();

void MouseLoop();

int GetCursorWidth();
int GetCursorHeight();

bool CursorInMenuArea(int cursorX, int cursorY);
bool CursorInFreqArea(int cursorX, int cursorY);
bool CursorInOpStatsArea(int cursorX, int cursorY);
bool CursorInAudioSpectrum(int cursorX, int cursorY);
bool CursorInSpectrumWaterfall(int cursorX, int cursorY);
bool CursorInInfoBox(int cursorX, int cursorY);
void MouseButtonFreqArea(int cursorX, int button);
void MouseWheelFreqArea(int cursorX, int wheel);
void MouseButtonOpStatsArea(int cursorX, int button);
