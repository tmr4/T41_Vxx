
/*

A T41 display driver must define the following display specific functions for a "standard" T41.
If the display doesn't impliment a given functionality, the function can be empty.

Required display specific functions:

  InitDisplay               - performs whatever initialization required by display
    * must include setting frequency spectrum noise floor and audio spectrum offset

  ShowSplash                - shows splash screen with up to 5 lines

  RedrawDisplayScreen       -

  // should consider USE_FULL_MENU
  EraseMenus                -
  ErasePrimaryMenu          -
  EraseSecondaryMenu        -

  DrawStaticDisplayItems    - draws static display items
  DrawSpectrumFrame         - draws spectrum display container
  DrawAudioSpectContainer   - draws audio spectrum box

  ShowFrequency             -
  ShowOperatingStats        -
  ShowCurrentPowerSetting   -
  ShowSpectrumdBScale       -
  ShowBandwidthBarValues    -
  ShowSpectrumFreqValues    -
  ShowTransmitReceiveStatus -
  ShowDecodedCW

  DrawFreqSpectrum          -
  DrawSmeterBar             -
  DrawAudioSpectrum         -
  DrawBandwidthBar          -
  DrawAudioFilterLines      -
  DrawCWFilter              -
  DrawCWDecoderLines        -

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
// Code
//-------------------------------------------------------------------------------------------------------------

// *** TODO: make this generic with pin assignments (currently tft is global object, but could make it a pointer) ***
void InitDisplay();

void ShowSplash(const char*line1Txt, const char*line2Txt, const char*line3Txt, const char*line4Txt, const char*line5Txt);

void RedrawDisplayScreen();
void EraseMenus();
void ErasePrimaryMenu();
void EraseSecondaryMenu();

// static items
void DrawStaticDisplayItems();
void DrawSpectrumFrame();
void DrawAudioSpectContainer();
//void DrawSMeterContainer();

//
void ShowFrequency();
void ShowOperatingStats();
void ShowCurrentPowerSetting();
void ShowSpectrumdBScale();
void ShowBandwidthBarValues();
void ShowSpectrumFreqValues();
void ShowTransmitReceiveStatus();
void ShowDecodedCW(char *buf);
void ShowSAM(float offset);

//
void DrawFreqSpectrum(bool newSpectrumFlag = false);
void DrawSmeterBar();
void DrawAudioSpectrum();
void DrawBandwidthBar();
void DrawAudioFilterLines();
void DrawCWFilter();
void DrawCWDecoderLines(int color);

//
void SetZoom(int zoom);
void SetWaterfallHeight(int pixels); // *** TODO: fix display dependent value ***
void ResetWaterfallHeight();



int GetDisplayWidth();
int GetDisplayHeight();
int GetCharWidth(int size);
int GetCharHeight(int size);

void DrawCursor(int cursorX, int cursorY, int oldCursorX, int oldCursorY);
void ReplaceCursor(int x, int y);
void CopyCursor(int x, int y);


// special functions used only in display related files
// these don't need a globally prototype
void DrawFT8Spectrum(uint8_t *spec, int numSamples, bool rollWaterfall = false);
void ShowFT8SpectrumFreqValues();
void DrawFT8BandwidthBar();

// erase various portions of the screen
void ClearScreen();
void EraseSpectrumDisplayContainer();
void EraseSpectrumWindow();

void MyDrawFloat(float val, int decimals, int x, int y, char *buff);
void MyDrawFloatP(float val, int decimals, int x, int y, char *buff, int width);

void PrintKeyboardBuffer();
