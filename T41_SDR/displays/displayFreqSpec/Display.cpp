#include <Audio.h>

#include "..\..\SDT.h"

//#include "Bearing.h"
//#include "..\..\Button.h"
#include "..\..\ButtonProc.h"
//#include "..\..\CW_Excite.h"
//#include "..\..\CWProcessing.h"
#include "Display.h"
#include "..\..\Display.h"
//#include "..\..\Encoders.h"
////#include "EEPROM.h"
//#include "..\..\Exciter.h"
//#include "..\..\Filter.h"
//#include "..\..\ft8.h"
//#include "InfoBox.h"
////#include "keyboard.h"
//#include "..\..\Menu.h"
//#include "..\..\MenuProc.h"
//#include "..\..\mouse.h"
//#include "..\..\Noise.h"
#include "..\..\Process.h"
//#include "..\..\Tune.h"
//#include "..\..\t41Control.h"
#include "..\..\Utility.h"
//
#include "..\..\debug.h"
//
//#include "..\..\keyboard.h"

//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

#define NEW_SI5351_FREQ_MULT  1UL
#define FLOAT_PRECISION         6             // Assumed precision for a float

//------------------------- Global Variables ----------

int displayState = DISPLAY_T41;

int centerLine = SPECTRUM_RES / 2 + SPECTRUM_LEFT_X;

int wfHeight = WATERFALL_H;

// *** TODO: consider defining spectrumNoiseFloor here as well ***
int audioSpectrumOffset;

#ifdef RA8875_DISPLAY
#define RA8875_CS TFT_CS
#define RA8875_RESET TFT_DC  // any pin or nothing!
#ifdef PROJECTSYSTEM
RA8875 tft = RA8875(RA8875_CS, RA8875_RESET, TFT_MOSI, TFT_SCLK, TFT_MISO);
#else
RA8875 tft = RA8875(RA8875_CS, RA8875_RESET);
#endif
#endif
#ifdef ILI9488_DISPLAY
ILI9488_t3 tft = ILI9488_t3(&SPI, TFT_CS, TFT_DC, TFT_RST);
#endif
#ifdef NO_DISPLAY
RA8875 tft = RA8875();
#endif

typedef struct {
  const char *dbText;
  float32_t   dBScale;
  uint16_t    pixelsPerDB;
  uint16_t    baseOffset;
  float32_t   offsetIncrement;
} dispSc;

dispSc displayScale[] =
{
  //                     not used                             not used
  // dbText    dBScale   pixelsPerDB   baseOffset             offsetIncrement
  { "20 dB/",  10.0,     2,             24,                   1.00 },
  { "10 dB/",  20.0,     4,            FREQSPEC_OFFSET_10DB,  0.50 }, // baseOffset calibrated to put peak at same level as audio spectrum (~3/4 scale) with AD3 (1mW -73dB external attenuation, 223.6mVrms @7.047MHz; see "Wavegen for RF in - S9 - 1mW with 73dB external atten.dwf3work")
  { " 5 dB/",  40.0,     8,             58,                   0.25 },
  { " 2 dB/",  100.0,    20,           120,                   0.10 },
  { " 1 dB/",  200.0,    40,           200,                   0.05 }
};

//int newSpectrumFlag = 0; // 0 - oldNF needs initialized in DrawFreqSpectrum(), 1 - it doesn't need initialized

//------------------------- Local Variables ----------
//uint8_t twinpeaks_tested = 2;  // this is never changed
//uint8_t write_analog_gain = 0; // this is never changed
int16_t pos_x_time = 390;
int16_t pos_y_time = 5;
int16_t spectrum_x = 10;

/* PROGMEM */ const uint16_t gradient[] = {  // Color array for waterfall background
  0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8, 0x9,
  0x10, 0x1F, 0x11F, 0x19F, 0x23F, 0x2BF, 0x33F, 0x3BF, 0x43F, 0x4BF,
  0x53F, 0x5BF, 0x63F, 0x6BF, 0x73F, 0x7FE, 0x7FA, 0x7F5, 0x7F0, 0x7EB,
  0x7E6, 0x7E2, 0x17E0, 0x3FE0, 0x67E0, 0x8FE0, 0xB7E0, 0xD7E0, 0xFFE0, 0xFFC0,
  0xFF80, 0xFF20, 0xFEE0, 0xFE80, 0xFE40, 0xFDE0, 0xFDA0, 0xFD40, 0xFD00, 0xFCA0,
  0xFC60, 0xFC00, 0xFBC0, 0xFB60, 0xFB20, 0xFAC0, 0xFA80, 0xFA20, 0xF9E0, 0xF980,
  0xF940, 0xF8E0, 0xF8A0, 0xF840, 0xF800, 0xF802, 0xF804, 0xF806, 0xF808, 0xF80A,
  0xF80C, 0xF80E, 0xF810, 0xF812, 0xF814, 0xF816, 0xF818, 0xF81A, 0xF81C, 0xF81E,
  0xF81E, 0xF81E, 0xF81E, 0xF83E, 0xF83E, 0xF83E, 0xF83E, 0xF85E, 0xF85E, 0xF85E,
  0xF85E, 0xF87E, 0xF87E, 0xF83E, 0xF83E, 0xF83E, 0xF83E, 0xF85E, 0xF85E, 0xF85E,
  0xF85E, 0xF87E, 0xF87E, 0xF87E, 0xF87E, 0xF87E, 0xF87E, 0xF87E, 0xF87E, 0xF87E,
  0xF87E, 0xF87E, 0xF87E, 0xF87E, 0xF88F, 0xF88F, 0xF88F
};

// FT8 waterfall gradient
// Simple color spectrum shifted toward red to highlight active channel
// FT8 spectrum value is 0-255, index to this is value/10
// *** 26 values here so 255/10 is a valid index ***
/* PROGMEM */ const uint16_t ft8Gradient[] = {  // Color array for FT8 waterfall background
  RA8875_BLACK,
  RA8875_BLACK,
  RA8875_BLACK,
  RA8875_BLACK,
  RA8875_BLACK,
  RA8875_BLACK,
  RA8875_BLACK,
  RA8875_BLACK,
  RA8875_BLUE,
  RA8875_CYAN,
  //RA8875_GREEN,
  //RA8875_YELLOW,
  //RA8875_LIGHT_ORANGE,
  //RA8875_DARK_ORANGE,
  //RA8875_DARK_ORANGE,
  RA8875_RED,
  RA8875_RED,
  RA8875_RED,
  RA8875_RED,
  RA8875_RED,
  RA8875_RED,
  RA8875_RED,
  RA8875_RED,
  RA8875_RED,
  RA8875_RED,
  RA8875_RED,
  RA8875_RED,
  RA8875_RED,
  RA8875_RED,
  RA8875_RED,
  RA8875_RED
};

int maxYPlot;
int filterWidthX;  // The current filter X.

struct DEMOD_Descriptor
{ const uint8_t DEMOD_n;
  const char* const text;
};
const DEMOD_Descriptor DEMOD[10] = {
  //   DEMOD_n, name
  { DEMOD_USB, "USB" },
  { DEMOD_LSB, "LSB" },
  { DEMOD_AM, "AM" },
  { DEMOD_SAM, "SAM" },
  { DEMOD_NFM, "NFM" },
  { DEMOD_FT8, "FT8" },
  { DEMOD_FT8_INTERNAL, "FT8.int" },
  { DEMOD_FT8_WAV, "FT8.wav" },
  { DEMOD_PSK31, "PSK31" },
  { DEMOD_PSK31_WAV, "PSK31.wav" },
};

//-------------------------------------------------------------------------------------------------------------
// Forwards
//-------------------------------------------------------------------------------------------------------------

void DrawSMeterContainer();

//-------------------------------------------------------------------------------------------------------------
// Code
//-------------------------------------------------------------------------------------------------------------

FLASHMEM void InitDisplay() {
  // set up display
  pinMode(TFT_MOSI, OUTPUT);
  digitalWrite(TFT_MOSI, HIGH);
  pinMode(TFT_SCLK, OUTPUT);
  digitalWrite(TFT_SCLK, HIGH);
  pinMode(TFT_CS, OUTPUT);
  digitalWrite(TFT_CS, HIGH);

  // *** is this setting the spi speed??? ***
  uint32_t iospeed_display = IOMUXC_PAD_DSE(3) | IOMUXC_PAD_SPEED(1);
  *(digital_pin_to_info_PGM + TFT_SCLK)->pad = iospeed_display;
  *(digital_pin_to_info_PGM + TFT_MOSI)->pad = iospeed_display;
  *(digital_pin_to_info_PGM + TFT_CS)->pad = iospeed_display;

  tft.begin(RA8875_800x480, 8, 20000000UL, 4000000UL);  // parameter list from library code

#ifdef DISPLAY_LANDSCAPE
  tft.setRotation(0); // connector on right
#endif
#ifdef DISPLAY_FLIPPED
  tft.setRotation(2); // connector on left
#endif

  // Setup for scrolling attributes. Part of initSpectrum_RA8875() call written by Mike Lewis
  tft.useLayers(true); // mainly used to turn on layers
  tft.layerEffect(OR); // overlay layers
  tft.writeTo(L2);
  tft.clearMemory();
  tft.writeTo(L1);
  tft.clearMemory();

  spectrumNoiseFloor = SPECTRUM_NOISE_FLOOR;
  audioSpectrumOffset = AUDIO_SPEC_SHIFT;
}

/*****
  Purpose: Update spectrum and waterfall on T41 display
            This is is a long running process.  It yields periodically to allow normal
            radio operations to continue.
*****/
FASTRUN void DrawFreqSpectrum(bool newSpectrumFlag /* = false */) {
  int yPlot, y1Plot;
  int hLo = 0, hHi = 0;
  int wfGradIndex;
  static int yOldPlot[SPECTRUM_RES];
  static int currentNF = 0;
  uint16_t waterfall[WATERFALL_W];
  int16_t pixelnew, pixelnew1;

  liveNoiseFloorFlag = 1;
  YieldToProcess(true);

  // set current noise flow level for this loop
  // noise floor is constant for each spectrum update
  // this allows live noise floor updates
  if(liveNoiseFloorFlag != 1) {
    currentNF = currentNoiseFloor[currentBand];
  }

  // initialize yOldPlot if this is a new spectrum
  // otherwise we use y values from last loop
  if(newSpectrumFlag) {
    memset(yOldPlot, SPECTRUM_BOTTOM, SPECTRUM_RES * sizeof(int));
    return; // *** TODO: check if this is needed ***
  }

  // Draw the frequency spectrums, gather data for waterfall
  for(int x1 = 0; x1 < SPECTRUM_RES - 1; x1++) {
    bool drawSpec = true, eraseSpec = true, inBoxLow = true, inBoxHigh = true;

    TOGGLEPROFILEPIN(PROFILER_DRAWFREQSPEC_PIN);

    pixelnew = displayScale[currentScale].baseOffset + bands[currentBand].pixelOffset + (int16_t) (displayScale[currentScale].dBScale * log10f_fast(freqSpecBuf[x1]));
    pixelnew1 = displayScale[currentScale].baseOffset + bands[currentBand].pixelOffset + (int16_t) (displayScale[currentScale].dBScale * log10f_fast(freqSpecBuf[x1 + 1]));

    // calculate the freq spectrum plot value
    yPlot = spectrumNoiseFloor - pixelnew - currentNF;
    y1Plot = spectrumNoiseFloor - pixelnew1 - currentNF;

    // create rough spectrum histogram if auto noise floor is active
    // the frequency spectrum is 150 pixels high, let's create
    // rough histogram 30 bins wide (or 5 pixels each, ie, divide by 5)
    // you might think divide by 4 would be more efficient as 2 right shifts
    // but right shift of a negative number is implimentation specific
    // and I want to keep the negative numbers here
    if(liveNoiseFloorFlag == 1) {
      int specPlotY = spectrumNoiseFloor - yPlot; // actual spectrum value at current noise floor
      int bin = specPlotY / 5;                    // divide by 5 to get histogram bin

      // hLo and hHi capture spectrum at or outside the spectrum display extremes
      // this is all we need to automatically set the noise floor
      // *** TODO: consider using other histogram bins to more rapidly set noise flow ***
      if(bin < 1) {
        hLo += 1;
      } else if(bin >= 29) {
        hHi += 1;
      }
    }

    // clear erase flag if we don't need to erase anything
    if((yOldPlot[x1] == SPECTRUM_BOTTOM) && (yOldPlot[x1 + 1] == SPECTRUM_BOTTOM)) {
      eraseSpec = false;
    }
    if((yOldPlot[x1] == SPECTRUM_TOP_Y) && (yOldPlot[x1 + 1] == SPECTRUM_TOP_Y)) {
      eraseSpec = false;
    }

    // erase the old spectrum if needed
    if(eraseSpec && (displayState == DISPLAY_T41)) {
      tft.drawLine(SPECTRUM_LEFT_X + x1, yOldPlot[x1 + 1], SPECTRUM_LEFT_X + x1, yOldPlot[x1], RA8875_BLACK);
    }

    // prevent drawing spectrum outside of the spectrum area
    // also clear draw flag if we don't need to draw anything
    if(yPlot > SPECTRUM_BOTTOM) {
      yPlot = SPECTRUM_BOTTOM;
      inBoxLow = false;
    }
    if(y1Plot > SPECTRUM_BOTTOM) {
      y1Plot = SPECTRUM_BOTTOM;
      drawSpec = inBoxLow ? true : false;
    }
    if(yPlot < SPECTRUM_TOP_Y) {
      yPlot = SPECTRUM_TOP_Y;
      inBoxHigh = drawSpec ? false : true;
    }
    if(y1Plot < SPECTRUM_TOP_Y) {
      y1Plot = SPECTRUM_TOP_Y;
      drawSpec = inBoxHigh ? true : false;
    }

    // draw the new spectrum if needed
    if(drawSpec && (displayState == DISPLAY_T41)) {
      tft.drawLine(SPECTRUM_LEFT_X + x1, y1Plot, SPECTRUM_LEFT_X + x1, yPlot, RA8875_YELLOW);
    }

    // save plot value to erase spectrum next loop
    yOldPlot[x1] = yPlot;

    #ifdef T41_REMOTE_DISPLAY
    if(connected) {
      freqData[x1] = yPlot;
    }
    #endif

    // create data for waterfall
    wfGradIndex = -yPlot + SPECTRUM_NOISE_FLOOR - 17;  // Nudged waterfall towards black
    if(wfGradIndex < 0) wfGradIndex = 0;
    if(wfGradIndex > 116) wfGradIndex = 116; // *** above is out of range of gradient ***
    waterfall[x1] = gradient[wfGradIndex];  // Try to put pixel values in middle of gradient array

    #ifdef NO_DISPLAY
    // along with the delay in the main loop this duplicates overall loop timing
    // with a display.  These are needed to regulate the flow of messages to the
    // PC control app.  These may not be needed if that app isn't used.
    delayMicroseconds(147);
    #endif

    YieldToProcess();
  }

  // save last plot value for erasing on next loop
  yOldPlot[SPECTRUM_RES - 1] = y1Plot;

  #ifdef T41_REMOTE_DISPLAY
    if(connected) {
      freqData[511] = pixelnew1;
    }
  #endif

  // adjust noise floor if auto noise floor is active
  if(liveNoiseFloorFlag == 1) {
    // auto noise floor give priority to ensuring the noise floor is visible in the lower portion of the spectrum display
    // the spectrum is 512 pixels wide, the noise floor is adjusted as follows (in order of priority):
    //    1) increase if more than 20% of the spectrum is in the bottom bin
    //    2) decrease if more than 5% is in the top bin
    //    3) decrease if less than 10% is in bottom bin
    // *** TODO: consider using other histogram bins to more rapidly set noise flow ***
    if(hLo > 102) {
      currentNF += 1;
    } else if((hHi > 25) || (hLo < 51)) {
      currentNF -= 1;
    }
  }

  // update noise floor sent to PC control app
  // *** data sent to PC no longer includes this as it is display dependent ***
  //if(controlDataFlag) {
  //  nf2PC = currentNF;
  //}

  // scroll the waterfall display
  // Use the Block Transfer Engine (BTE) to move waterfall down a line
  // copy the waterfall between layers in a DMA ping/pong manner, moving it down to row 2
  if(displayState == DISPLAY_T41) {
    static int tik = 1, tok = 2;

    tft.BTE_move(WATERFALL_L, WATERFALL_T, WATERFALL_W, wfHeight, WATERFALL_L, WATERFALL_T + 1, tik, tok);
    tft.readStatus(); // Make sure it is done.  Memory moves can take time. This is blocking. *** might need to be changed back to original if blocking nature is modified ***
    if(tik == 1) {
      tik = 2;
      tok = 1;
      tft.writeTo(L2);
    } else {
      tik = 1;
      tok = 2;
      tft.writeTo(L1);
    }

    // write new row of data into the top row to finish the scrolling effect
    tft.writeRect(WATERFALL_L, WATERFALL_T, WATERFALL_W, 1, waterfall);
    tft.writeTo(L1);
  }

  RESETPROFILEPIN(PROFILER_DRAWFREQSPEC_PIN);
}
