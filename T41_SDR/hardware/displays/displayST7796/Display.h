
#include "..\..\T41Config.h"

#include <SPI.h>
#include <ST7796_t3.h>                    // https://github.com/mjs513/RA8875/tree/ILI9341_t4

//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

#define XPIXELS                     480
#define YPIXELS                     320
#define CHAR_HEIGHT                  32
#define PIXELS_PER_EQUALIZER_DELTA   10           // Number of pixels per detent of encoder for equalizer changes
#define PIXELS_PER_AUDIO_DELTA       10

#define FREQUENCY_X           2
#define FREQUENCY_Y           2
#define FREQUENCY_X_SPLIT     (XPIXELS / 2 - 50)
#define VFO_B_ACTIVE_OFFSET   FREQUENCY_X_SPLIT - 60
#define VFO_B_INACTIVE_OFFSET FREQUENCY_X_SPLIT + 60

#define OPERATION_STATS_L     FREQUENCY_Y
#define OPERATION_STATS_T     (FREQUENCY_Y + 30)
#define OPERATION_STATS_W     (310 + 2 - OPERATION_STATS_L)
#define OPERATION_STATS_H     25

#define OPERATION_STATS_CF    25  // center frequency
#define OPERATION_STATS_BD    80  // band
#define OPERATION_STATS_MD    120 // mode
#define OPERATION_STATS_CWF   150 // CW filter
#define OPERATION_STATS_DMD   190 // demod mode
#define OPERATION_STATS_PWR   250 // power level

#define SPECTRUM_RES          479
#define SPECTRUM_HEIGHT       100

#define SPEC_BOX_L            2
#define SPEC_BOX_T            50
#define SPEC_BOX_W            SPECTRUM_RES + 4
#define SPEC_BOX_H            SPECTRUM_HEIGHT + 4

#define SPECTRUM_LEFT_X       SPEC_BOX_L + 2            // Used to plot left edge of spectrum display
#define SPECTRUM_TOP_Y        SPEC_BOX_T + 2            // Start of spectrum plot space
#define SPECTRUM_BOTTOM       SPECTRUM_TOP_Y + SPECTRUM_HEIGHT - 4

#define SPECTRUM_NOISE_FLOOR  (SPECTRUM_TOP_Y + SPECTRUM_HEIGHT - 3)
#define SPEC_BOX_LABELS       (SPECTRUM_TOP_Y + SPECTRUM_HEIGHT + 10)

#define FILTER_PARAMETERS_X   5 //(XPIXELS * 0.22)
#define FILTER_PARAMETERS_Y   (SPECTRUM_TOP_Y + 2) //(YPIXELS * 0.213)

#define WATERFALL_L           2
#define WATERFALL_T           (SPEC_BOX_LABELS+20)
#define WATERFALL_BOTTOM      YPIXELS
#define WATERFALL_W           SPECTRUM_RES
#define WATERFALL_H           (WATERFALL_BOTTOM-WATERFALL_T)

#define X_R_STATUS_X          (XPIXELS - 55)
#define X_R_STATUS_Y          0

#define SMETER_CONTAINER_X    (SPECTRUM_LEFT_X + SPECTRUM_RES + 15)
#define SMETER_CONTAINER_Y    25
#define SMETER_BAR_X          (SMETER_CONTAINER_X + 1)
#define SMETER_BAR_Y          (SMETER_CONTAINER_Y + 2)
#define SMETER_BAR_HEIGHT     16
#define SMETER_BAR_LENGTH     180

#define AUDIO_SPEC_BOX_L      (SPECTRUM_LEFT_X + SPECTRUM_RES + 15)
#define AUDIO_SPEC_BOX_T      OPERATION_STATS_T
#define AUDIO_SPEC_BOX_W      (XPIXELS - AUDIO_SPEC_BOX_L) // use up rest of screen right
#define AUDIO_SPEC_BOX_H      118
#define AUDIO_SPEC_BOX_BOTTOM (OPERATION_STATS_T + AUDIO_SPEC_BOX_H)

#define AUDIO_SPEC_RES        (AUDIO_SPEC_BOX_W - 2)
#define AUDIO_SPEC_L          AUDIO_SPEC_BOX_L + 1
#define AUDIO_SPEC_T          AUDIO_SPEC_BOX_T + 1
#define AUDIO_SPEC_H          AUDIO_SPEC_BOX_H - 2
#define AUDIO_SPEC_W          AUDIO_SPEC_BOX_W - 2
#define AUDIO_SPEC_BOTTOM     AUDIO_SPEC_BOX_BOTTOM - 2
#define AUDIO_SPEC_SPAN       6250.0

#define CLIP_AUDIO_PEAK       115           // The pixel value where audio peak overwrites S-meter

// info box coordinates and item identifiers
#define INFO_BOX_L            (SPECTRUM_LEFT_X + SPECTRUM_RES + 15)
#define INFO_BOX_T            (AUDIO_SPEC_BOX_BOTTOM + 25) //(SPECTRUM_TOP_Y + SPECTRUM_HEIGHT + 40)
#define INFO_BOX_W            XPIXELS - INFO_BOX_L // use up remainder of screen right
#define INFO_BOX_H            YPIXELS - INFO_BOX_T // use up remainder of screen bottom

#define TIME_X                INFO_BOX_L + 10       // Upper-left corner for time
#define TIME_Y                INFO_BOX_T + 2

#define DEFAULT_EQUALIZER_BAR 100                                         // Default equalizer bar height
#define VFOA_PIXEL_LENGTH     275
#define VFOB_PIXEL_LENGTH     280
#define FREQUENCY_PIXEL_HI    45
#define SPLIT_INCREMENT       500L

// commented out colors are predefined
#define  BLACK                    0x0000      /*   0,   0,   0 */
//#define  ILI9341_BLUE              0x000F      /*   0,   0, 128 */
#define  DARK_GREEN               0x03E0      /*   0, 128,   0 */
#define  DARKCYAN                 0x03EF      /*   0, 128, 128 */
#define  MAROON                   0x7800      /* 128,   0,   0 */
#define  PURPLE                   0x780F      /* 128,   0, 128 */
#define  OLIVE                    0x7BE0      /* 128, 128,   0 */
//#define  ILI9341_LIGHT_GREY        0xC618      /* 192, 192, 192 */
#define  DARK_RED                 tft.Color565(64,0,0)
#define  DARKGREY                 0x7BEF      /* 128, 128, 128 */
#define  BLUE                     0x001F      /*   0,   0, 255 */
//#define  ILI9341_GREEN             0x07E0      /*   0, 255,   0 */
#define  CYAN                     0x07FF      /*   0, 255, 255 */
#define  RED                      0xF800      /* 255,   0,   0 */
#define  MAGENTA                  0xF81F      /* 255,   0, 255 */
#define  YELLOW                   0xFFE0      /* 255, 255,   0 */
#define  WHITE                    0xFFFF      /* 255, 255, 255 */
#define  ORANGE                   0xFD20      /* 255, 165,   0 */
//#define  ILI9341_GREENYELLOW       0xAFE5      /* 173, 255,  47 */
#define  PINK                     0xF81F
#define  FILTER_WIN               0x10       // Color of SSB filter width

extern int displayState;

extern ST7796_t3 tft;

//-------------------------------------------------------------------------------------------------------------
// Code
//-------------------------------------------------------------------------------------------------------------
/*
void InitDisplay();

// static items
void DrawSpectrumFrame();
//void DrawSMeterContainer();

void DrawStaticDisplayItems();

void DrawAudioSpectContainer();

// mainly updated each loop during call to DrawFreqSpectrum
void ShowFrequency(bool includeInactiveVFO = false);
void ShowOperatingStats();
void ShowSpectrumFreqValues();
void DrawSmeterBar();
void DrawBandwidthBar();
void ShowBandwidthBarValues();
void DrawAudioFilterLines();

//
void ShowSpectrumdBScale();
void ShowTransmitReceiveStatus();
void ShowCurrentPowerSetting();

//void DrawFreqSpectrum(bool newSpectrumFlag = false);
void DrawAudioSpectrum();

//void DrawFT8Spectrum(uint8_t *spec, int numSamples, bool rollWaterfall = false);
void ShowFT8SpectrumFreqValues();
void DrawFT8BandwidthBar();

void DrawCWFilter();

void RedrawDisplayScreen();

// erase various portions of the screen
void ClearScreen();
void EraseSpectrumDisplayContainer();
void EraseMenus();
void ErasePrimaryMenu();
void EraseSecondaryMenu();
void EraseSpectrumWindow();

void MyDrawFloat(float val, int decimals, int x, int y, char *buff);
void MyDrawFloatP(float val, int decimals, int x, int y, char *buff, int width);

void PrintKeyboardBuffer();

void SetWaterfallHeight(int pixels);
void ResetWaterfallHeight();
*/
