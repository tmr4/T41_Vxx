
/*

A T41 display driver must define the display specific functions listed in the Forwards
section for a standard operating T41. If the display doesn't impliment a given
functionality, the function body can be empty.

Display specific functions: *** TODO: complete summary ***

  InitDisplay               - performs whatever initialization required by display
    * must include setting frequency spectrum noise floor and audio spectrum offset

  ShowSplash                - shows splash screen with up to 5 lines

  DrawStaticDisplayItems    - draws static display items
  DrawSpectrumFrame         - draws spectrum display container
  DrawAudioSpectContainer   - draws audio spectrum box

*/

//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

#define DISPLAY_T41                 0
#define DISPLAY_T41_FT8_DECODE      1
#define DISPLAY_BEACON_MONITOR      2
#define DISPLAY_FULL_MENU           3
#define DISPLAY_CALIBRATION         4

extern int displayState;

extern int centerLine;

//-------------------------------------------------------------------------------------------------------------
// Forwards
//-------------------------------------------------------------------------------------------------------------

/*

The following routines are referenced by the common and/or current hardware specific code.  The functions are
defined in display specific modules and can be empty allowing the T41 core code, with the selected hardware
version, to compile and run without a given feature or even a display.
To create a new display module, impilement the desired features from the functions below and the core code will
call the routines at the appropriate time.  The InitDisplay function is required at a minimum for an operating display.

*/

// General
void InitDisplay();

void ClearScreen();

int GetDisplayWidth();
int GetDisplayHeight();
int GetCharWidth(int);
int GetCharHeight(int);

void ShowSplash(char const*, char const*, char const*, char const*, char const*);
void ShowNoSD();
void ShowDot();

// main T41 display
void DrawStaticDisplayItems();
void EraseSpectrumWindow();
void RedrawDisplayScreen();
void ShowFrequency();
void ShowOperatingStats();
void ShowSAM(float);
void ShowCurrentPowerSetting();
void DrawSpectrumFrame();
void EraseSpectrumDisplayContainer();
void ShowSpectrumFreqValues();
void DrawFreqSpectrum(bool newSpectrumFlag = false);
void ShowSpectrumdBScale();
void ShowBandwidthBarValues();
void DrawBandwidthBar();
void SetZoom(int);
void SetWaterfallHeight(int);
void ResetWaterfallHeight();
void ShowTransmitReceiveStatus();
void DrawSmeterBar();
void DrawAudioSpectContainer();
void DrawAudioSpectrum();
void DrawCWFilter();
void DrawAudioFilterLines();
void DrawCWDecoderLines(int);
void ShowDecodedCW(char*);

void MyDrawFloat(float val, int decimals, int x, int y, char *buff);
void MyDrawFloatP(float val, int decimals, int x, int y, char *buff, int width);

// info box
void UpdateInfoBox();
void UpdateInfoBoxItem(unsigned char);
void HighlightIBItem(unsigned char, int);
void UpdateIBWPM();
void ClearInfoBoxKeyer();
void UpdateDecodeLockIndicator();
void UpdateClock();

// menu
void ShowMenu(char const**, int);
void EraseMenus();
void ErasePrimaryMenu();
void EraseSecondaryMenu();
void ShowMenuItem(char const*);
void ShowMenuItemValue(int, int, char const*);
void ProcessEqualizerChoices(int, char*);
void GetFavoriteFrequency();
void SetFavoriteFrequency();

// mouse
int GetCursorWidth();
int GetCursorHeight();
void CopyCursor(int, int);
void DrawCursor(int, int, int, int);
void ReplaceCursor(int, int);
void SetFtActive(int);
bool CursorInMenuArea(int cursorX, int cursorY);
bool CursorInFreqArea(int cursorX, int cursorY);
bool CursorInOpStatsArea(int cursorX, int cursorY);
bool CursorInAudioSpectrum(int cursorX, int cursorY);
bool CursorInSpectrumWaterfall(int cursorX, int cursorY);
bool CursorInInfoBox(int cursorX, int cursorY);
void MouseButtonFreqArea(int cursorX, int button);
void MouseWheelFreqArea(int cursorX, int wheel);
void MouseButtonOpStatsArea(int cursorX, int button);
void MouseButtonInfoBox(int, int, int);
void MouseWheelInfoBox(int, int, int);

// *** other features ***
// FT8
void InitFT8Display();
int GetRow(int);
void DisplayMessages(int, int*, int, bool, int&, int, int);
void DisplayQSO(int);
void DisplaySelectedMessageDetail();
void DrawFT8Spectrum(unsigned char*, int, bool);
void ShowFT8SpectrumFreqValues();
void DisplayStats(int, int, int, int, bool);
void DrawFT8BandwidthBar();

// Beacon Monitor
void BeaconInit();
void BeaconExit();
void BeaconLoop();

// Bearing Map
void BearingMaps();
void ButtonBearing();

/*
void PrintKeyboardBuffer();
*/
