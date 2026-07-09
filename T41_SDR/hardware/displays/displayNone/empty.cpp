
//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

bool beaconFlag;
int cursorW, cursorH;

int centerLine;

//-------------------------------------------------------------------------------------------------------------
// Forwards
//-------------------------------------------------------------------------------------------------------------

void YieldToProcess(bool updateSpectrum = false);

//-------------------------------------------------------------------------------------------------------------
// Code
//-------------------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------------------
/*

The following routines are referenced by the common and/or current hardware specific code.  The functions are
empty here allowing the T41 core code, with the selected hardware version, to compile and run without a display.
To create a new display module, impilement the desired feature from the functions below and the core code will
call the routine at the appropriate time.  The InitDisplay function must be defined at a minimum.

*/
//-------------------------------------------------------------------------------------------------------------

// General
void InitDisplay() {}

void ClearScreen() {}

int GetDisplayWidth() { return 0; }
int GetDisplayHeight() { return 0; }
int GetCharWidth(int) { return 0; }
int GetCharHeight(int) { return 0; }

void ShowSplash(char const*, char const*, char const*, char const*, char const*) {}
void ShowNoSD() {}
void ShowDot() {}

void ShowRemoteStatus() {}

// main T41 display
void DrawStaticDisplayItems() {}
void EraseSpectrumWindow() {}
void RedrawDisplayScreen() {}
void ShowFrequency(bool) {}
void ShowOperatingStats() {}
void ShowSAM(float) {}
void ShowCurrentPowerSetting() {}
void DrawSpectrumFrame() {}
void EraseSpectrumDisplayContainer() {}
void ShowSpectrumFreqValues() {}
void DrawFreqSpectrum(bool newSpectrumFlag /* = false */) { YieldToProcess(); }
void ShowSpectrumdBScale() {}
void ShowBandwidthBarValues() {}
void DrawBandwidthBar() {}
void SetWaterfallHeight(int) {}
void ResetWaterfallHeight() {}
void ShowTransmitReceiveStatus() {}
void DrawSmeterBar() {}
void DrawAudioSpectContainer() {}
void DrawAudioSpectrum() { YieldToProcess(); }
void DrawCWFilter() {}
void DrawAudioFilterLines() {}
void DrawCWDecoderLines(int) {}
void ShowDecodedCW(char*) {}

void UpdateLiveDisplayAreas() {}

void MyDrawFloat(float val, int decimals, int x, int y, char *buff) {}
void MyDrawFloatP(float val, int decimals, int x, int y, char *buff, int width) {}

// info box
void UpdateInfoBox() {}
void UpdateInfoBoxItem(int) {}
void HighlightIBItem(unsigned char, int) {}
void UpdateIBWPM() {}
void ClearInfoBoxKeyer() {}
void UpdateDecodeLockIndicator() {}
void UpdateClock() {}
void HighlightTuneInc() {}

// menu
void ShowMenu(char const**, int) {}
void EraseMenus() {}
void ErasePrimaryMenu() {}
void EraseSecondaryMenu() {}
void ShowMenuItem(char const*) {}
void ShowMenuItemValue(int, int, char const*) {}
void ProcessEqualizerChoices(int, char*) {}

// mouse
int GetCursorWidth() { return 0; }
int GetCursorHeight() { return 0; }
void CopyCursor(int, int) {}
void DrawCursor(int, int, int, int) {}
void ReplaceCursor(int, int) {}
bool CursorInMenuArea(int cursorX, int cursorY) { return false; }
bool CursorInFreqArea(int cursorX, int cursorY) { return false; }
bool CursorInOpStatsArea(int cursorX, int cursorY) { return false; }
bool CursorInAudioSpectrum(int cursorX, int cursorY) { return false; }
bool CursorInSpectrumWaterfall(int cursorX, int cursorY) { return false; }
bool CursorInInfoBox(int cursorX, int cursorY) { return false; }
void MouseButtonFreqArea(int cursorX, int button) {}
void MouseWheelFreqArea(int cursorX, int wheel) {}
void MouseButtonOpStatsArea(int cursorX, int button) {}
void MouseButtonInfoBox(int, int, int) {}
void MouseWheelInfoBox(int, int, int) {}

// *** other features ***
// FT8
void InitFT8Display() {}
int GetRow(int) { return 0; }
void DisplayMessages(int, int*, int, bool, int&, int, int) {}
void DisplayQSO(int) {}
void DisplaySelectedMessageDetail() {}
void DrawFT8Spectrum(unsigned char*, int, bool) {}
void ShowFT8SpectrumFreqValues() {}
void DisplayStats(int, int, int, int, bool) {}
void DrawFT8BandwidthBar() {}

// Beacon Monitor
void BeaconInit() {}
void BeaconExit() {}
void BeaconLoop() {}

// Bearing Map
void BearingMaps() {}
void ButtonBearing() {}

/*
void PrintKeyboardBuffer() {}
*/
