
//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

bool infoBoxItemActive[24];
bool beaconFlag;
int cursorW, cursorH;

int displayState = -1; // no display
int centerLine;

//-------------------------------------------------------------------------------------------------------------
// Code
//-------------------------------------------------------------------------------------------------------------

void BeaconInit() {}
void BeaconExit() {}
void UpdateInfoBox() {}
void UpdateInfoBoxItem(unsigned char) {}
void UpdateIBWPM() {}
void SetWaterfallHeight(int) {}
void ShowMenu(char const**, int) {}
void ShowMenuItem(char const*) {}
void ShowMenuItemValue(int, int, char const*) {}
void ProcessEqualizerChoices(int, char*) {}
void GetFavoriteFrequency() {}
void ButtonBearing() {}
void BeaconLoop() {}
void SetFtActive(int) {}
void ClearInfoBoxKeyer() {}
void UpdateDecodeLockIndicator() {}
void BearingMaps() {}
void UpdateClock() {}
void MouseWheelInfoBox(int, int, int) {}
void SetFavoriteFrequency() {}
void HighlightIBItem(unsigned char, int) {}
int GetCursorWidth() { return 0; }
int GetCursorHeight() { return 0; }
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
void InitDisplay() {}
void DrawFreqSpectrum(bool newSpectrumFlag /* = false */) {}
void SetZoom(int) {}
void ResetWaterfallHeight() {}
void ShowBandwidthBarValues() {}
void ShowOperatingStats() {}
void DrawCWFilter() {}
void DrawAudioFilterLines() {}
void DrawCWDecoderLines(int) {}
void ShowDecodedCW(char*) {}
void RedrawDisplayScreen() {}
void EraseMenus() {}
void ShowCurrentPowerSetting() {}
void ShowSpectrumdBScale() {}
void ShowFrequency() {}
void ShowSplash(char const*, char const*, char const*, char const*, char const*) {}
void DrawStaticDisplayItems() {}
void ShowTransmitReceiveStatus() {}
void EraseSpectrumDisplayContainer() {}
void DrawFT8BandwidthBar() {}
void ReplaceCursor(int, int) {}
void ShowSAM(float) {}
void DrawBandwidthBar() {}
int GetDisplayWidth() { return 0; }
int GetDisplayHeight() { return 0; }
int GetCharWidth(int) { return 0; }
int GetCharHeight(int) { return 0; }
void DrawAudioSpectContainer() {}
void EraseSecondaryMenu() {}
void ShowSpectrumFreqValues() {}
void DrawSpectrumFrame() {}
void DrawAudioSpectrum() {}
void CopyCursor(int, int) {}
void DrawCursor(int, int, int, int) {}
void DrawFT8Spectrum(unsigned char*, int, bool) {}
void ShowFT8SpectrumFreqValues() {}

void DisplayStats(int, int, int, int, bool) {}
void DisplayMessages(int, int*, int, bool, int&, int, int) {}
void EraseSpectrumWindow() {}
void DisplayQSO(int) {}
void DisplaySelectedMessageDetail() {}
void InitFT8Display() {}
int GetRow(int) { return 0; }
