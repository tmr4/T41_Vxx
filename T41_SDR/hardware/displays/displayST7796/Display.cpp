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
#include "..\..\Menu.h"
//#include "..\..\MenuProc.h"
//#include "..\..\mouse.h"
//#include "..\..\Noise.h"
#include "..\..\Process.h"
#include "..\..\Tune.h"
//#include "..\..\t41Control.h"
#include "..\..\Utility.h"
//
#include "..\..\debug.h"
//
//#include "..\..\keyboard.h"

#include <ST7796_t3.h>
#include <st7735_t3_font_Arial.h>

//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

#define NEW_SI5351_FREQ_MULT  1UL
#define FLOAT_PRECISION         6             // Assumed precision for a float

//------------------------- Global Variables ----------

int displayState = DISPLAY_T41;

int centerLine = SPECTRUM_RES / 2 + SPECTRUM_LEFT_X;

int wfHeight = WATERFALL_H;

// Current draw for T41 mock up on Mini Platform, 0.2 amps (ST7796 480x320)
ST7796_t3 tft = ST7796_t3(TFT_CS, TFT_DC);

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
  ST7735_BLACK,
  ST7735_BLACK,
  ST7735_BLACK,
  ST7735_BLACK,
  ST7735_BLACK,
  ST7735_BLACK,
  ST7735_BLACK,
  ST7735_BLACK,
  ST7735_BLUE,
  ST7735_CYAN,
  //ST7735_GREEN,
  //ST7735_YELLOW,
  //ST7735_LIGHT_ORANGE,
  //ST7735_DARK_ORANGE,
  //ST7735_DARK_ORANGE,
  ST7735_RED,
  ST7735_RED,
  ST7735_RED,
  ST7735_RED,
  ST7735_RED,
  ST7735_RED,
  ST7735_RED,
  ST7735_RED,
  ST7735_RED,
  ST7735_RED,
  ST7735_RED,
  ST7735_RED,
  ST7735_RED,
  ST7735_RED,
  ST7735_RED,
  ST7735_RED
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
  tft.init(320, 480);

  tft.invertDisplay(true);  // LCD requires colors to be inverted
  tft.setRotation(3);       // Rotates screen to match the baseboard orientation
  //delay(1000);

  // test screen
  tft.fillScreen(ST7735_BLACK);
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
  static uint16_t waterfall[60][WATERFALL_W] = {0}; // circular buffer
  static int head = 0;
  int16_t pixelnew, pixelnew1;
  int offset = (512-SPECTRUM_RES) / 2;
  bool init = false;

  // initialize yOldPlot if this is a new spectrum
  // otherwise we use y values from last loop
  if(newSpectrumFlag) {
    memset(yOldPlot, SPECTRUM_BOTTOM, SPECTRUM_RES * sizeof(int));
    return; // *** TODO: check if this is needed ***
  }

  YieldToProcess(true);

  // set current noise flow level for this loop
  // noise floor is constant for each spectrum update
  // this allows live noise floor updates
  if(t41.LiveNoiseFloor != 1) {
    currentNF = t41.NoiseFloor;
  }

  // Draw the frequency spectrums, gather data for waterfall
  for(int x1 = 0; x1 < SPECTRUM_RES - 1; x1++) {
    bool drawSpec = true, eraseSpec = true, inBoxLow = true, inBoxHigh = true;

    TOGGLEPROFILEPIN(PROFILER_DRAWFREQSPEC);

    pixelnew = displayScale[t41.FreqSpecScale].baseOffset + bands[t41.ActiveBand].pixelOffset + (int16_t) (displayScale[t41.FreqSpecScale].dBScale * log10f_fast(freqSpecBuf[x1 + offset]));
    pixelnew1 = displayScale[t41.FreqSpecScale].baseOffset + bands[t41.ActiveBand].pixelOffset + (int16_t) (displayScale[t41.FreqSpecScale].dBScale * log10f_fast(freqSpecBuf[x1 + 1 + offset]));

    // calculate the freq spectrum plot value
    yPlot = SPECTRUM_NOISE_FLOOR - pixelnew - currentNF;
    y1Plot = SPECTRUM_NOISE_FLOOR - pixelnew1 - currentNF;

    // create rough spectrum histogram if auto noise floor is active
    // the frequency spectrum is 150 pixels high, let's create
    // rough histogram 30 bins wide (or 5 pixels each, ie, divide by 5)
    // you might think divide by 4 would be more efficient as 2 right shifts
    // but right shift of a negative number is implimentation specific
    // and I want to keep the negative numbers here
    if(t41.LiveNoiseFloor == 1) {
      int specPlotY = SPECTRUM_NOISE_FLOOR - yPlot; // actual spectrum value at current noise floor
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
      tft.drawLine(SPECTRUM_LEFT_X + x1, yOldPlot[x1 + 1], SPECTRUM_LEFT_X + x1, yOldPlot[x1], ST7735_BLACK);
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
      tft.drawLine(SPECTRUM_LEFT_X + x1, y1Plot, SPECTRUM_LEFT_X + x1, yPlot, ST7735_YELLOW);
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
    waterfall[head][x1] = gradient[wfGradIndex];  // Try to put pixel values in middle of gradient array

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
  if(t41.LiveNoiseFloor == 1) {
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
  //if(displayState == DISPLAY_T41) {
  //  static int tik = 1, tok = 2;
  //
  //  tft.BTE_move(WATERFALL_L, WATERFALL_T, WATERFALL_W, wfHeight, WATERFALL_L, WATERFALL_T + 1, tik, tok);
  //  tft.readStatus(); // Make sure it is done.  Memory moves can take time. This is blocking. *** might need to be changed back to original if blocking nature is modified ***
  //  if(tik == 1) {
  //    tik = 2;
  //    tok = 1;
  //    tft.writeTo(L2);
  //  } else {
  //    tik = 1;
  //    tok = 2;
  //    tft.writeTo(L1);
  //  }
  //
  //  // write new row of data into the top row to finish the scrolling effect
  //  tft.writeRect(WATERFALL_L, WATERFALL_T, WATERFALL_W, 1, waterfall);
  //  tft.writeTo(L1);
  //}

  // draw the rest of the waterfall
  int j = head;
  for(int i = 0; i < wfHeight; i++) {
    tft.writeRect(WATERFALL_L, WATERFALL_T + i, WATERFALL_W, 1, waterfall[j--]);
    if(j<0) j = 59;
    YieldToProcess();
  }
  if(++head >= 60) head = 0;

  RESETPROFILEPIN(PROFILER_DRAWFREQSPEC);
  tft.drawRect(SPEC_BOX_L, SPEC_BOX_T, SPEC_BOX_W, SPEC_BOX_H, ST7735_YELLOW);
}

/*****
  Purpose: draws spectrum display container
*****/
FLASHMEM void DrawSpectrumFrame() {
  tft.drawRect(SPEC_BOX_L, SPEC_BOX_T, SPEC_BOX_W, SPEC_BOX_H, ST7735_YELLOW);
}

/*****
  Purpose: This function draws the frequency bar at the bottom of the spectrum scope, putting markers at every
            graticule and the full frequency
*****/
FLASHMEM void ShowSpectrumFreqValues() {
  char txt[16];
  int pos_help, tickX;
  int tunedInx = 0;
  float cFreq = (float)t41.CenterFreq;
  float tunedFreq, lFreq;
  float fInc =  t41.SampleRate / (float)(1 << t41.SpectrumZoom) / 4.0;
  // positions for graticules: first for t41.SpectrumZoom < 3, then for t41.SpectrumZoom > 2
  //const static int idx2pos[2][9] = {
  //  { -43, 21, 50, 250, 140, 250, 232, 250, 315 },
  //  { -43, 21, 50, 85, 200, 200, 232, 218, 315 }
  //};
  const static int idx2pos[2][9] = {
    { 0, 0, 25, 0, 0, 0, 100, 0, 125 },
    { 0,0,0,0,0,0,0,0,0 }
  };
  float xExpand = 1.4;
  float32_t pixel_per_hz = (1 << t41.SpectrumZoom) * SPECTRUM_RES / t41.SampleRate;

  //tft.setFontScale((enum RA8875tsize)0);
  //tft.setFont(Arial_12);
  tft.setFont(Arial_8);

  // erase frequency bar values and tick marks
  //tft.fillRect(SPECTRUM_LEFT_X, SPEC_BOX_LABELS - 4, SPECTRUM_RES + 5, tft.getFontHeight() + 4, ST7735_BLACK);
  tft.fillRect(SPECTRUM_LEFT_X, SPEC_BOX_LABELS - 4, SPECTRUM_RES + 5, 12, ST7735_BLACK);

  if(t41.SpectrumZoom == 0) {
    tunedInx = -1;
    cFreq += t41.IntermediateFreq;
    tft.setCursor(centerLine - 140, SPEC_BOX_LABELS);
  } else {
    //tft.setCursor(centerLine - 20, SPEC_BOX_LABELS);
    tft.setCursor(centerLine - 10, SPEC_BOX_LABELS);
  }

  if(t41.DemodMode == DEMOD_FT8) {
    //tunedInx = -1;
    //cFreq += fInc;
    //tft.setCursor(centerLine - 140, SPEC_BOX_LABELS);
  }

  // calc tuned frequency and it's rounded (label) value
  // calc the position of the tick mark for the label value
  tunedFreq = (cFreq + (float)tunedInx * fInc);
  lFreq = round((cFreq + (float)tunedInx * fInc) / 1000.0) * 1000.0;
  ultoa(lFreq / 1000.0, txt, DEC);
  tickX = (tunedFreq - lFreq - t41.IntermediateFreq * tunedInx) * pixel_per_hz;

  // print label and tick mark
  tft.setTextColor(ST7735_GREEN);
  tft.print(txt);
  //tft.drawFastVLine(SPECTRUM_LEFT_X + centerLine - tickX, SPEC_BOX_LABELS - 4, 6, ST7735_YELLOW);
  tft.drawFastVLine(centerLine - tickX, SPEC_BOX_LABELS - 8, 6, ST7735_YELLOW);

  // print non-center freq and tick marks
  tft.setTextColor(ST7735_WHITE);
  //for(int idx = -2; idx < 3; idx++) {
  for(int idx = -2; idx < 3; idx++) {
    //pos_help = idx2pos[t41.SpectrumZoom < 3 ? 0 : 1][idx * 2 + 4];
    //if(idx != tunedInx) {
    if(idx != tunedInx) {
      // calculate label freq (always a whole number) and the exact position of its tick mark
      lFreq = round((cFreq +  (float)idx * fInc) / 1000.0) * 1000.0;
      ultoa(lFreq / 1000.0, txt, DEC);
      tickX = (tunedFreq - lFreq - t41.IntermediateFreq * tunedInx) * pixel_per_hz;

      // print freq label (always in the same position for visual)
      if(idx < 2) {
        //tft.setCursor(SPECTRUM_LEFT_X + pos_help * xExpand + 40, SPEC_BOX_LABELS);
        tft.setCursor(centerLine + idx * (SPECTRUM_RES / 4) - 10, SPEC_BOX_LABELS);
      } else {
        //tft.setCursor(SPECTRUM_LEFT_X + (pos_help + 9) * xExpand + 59 - strlen(txt)*tft.getFontWidth(), SPEC_BOX_LABELS);
        tft.setCursor(centerLine + idx * (SPECTRUM_RES / 4) - strlen(txt)*6, SPEC_BOX_LABELS);
      }
      tft.print(txt);

      // print label tick mark for the rounded freq label
      //tft.drawFastVLine(SPECTRUM_LEFT_X + centerLine - tickX, SPEC_BOX_LABELS - 4, 6, ST7735_YELLOW);
      tft.drawFastVLine(centerLine - tickX, SPEC_BOX_LABELS - 8, 6, ST7735_YELLOW);
    }
  }
}

/*****
  Purpose: Show main frequency display at top
*****/
FASTRUN void ShowFrequency() {
  char freqBuffer[15];
  int freq = t41.ActiveFreq();
  int color = ST7735_GREEN;
  int x = t41.ActiveVFO == VFO_A ? FREQUENCY_X : VFO_B_ACTIVE_OFFSET;

  // show active frequency
  //tft.setFontScale(3, 2);
  tft.setFont(Arial_20);
  //tft.fillRect(x, FREQUENCY_Y, 15 * tft.getFontWidth(), tft.getFontHeight(), RA8875_BLACK);
  tft.fillRect(x, FREQUENCY_Y, 15*8, 20*8, ST7735_BLACK);

  if(freq < bands[t41.ActiveBand].fBandLow || freq > bands[t41.ActiveBand].fBandHigh) {
    color = ST7735_RED; // Out of band
  }

  tft.setTextColor(color);
  FormatFrequency(freq, freqBuffer);
  tft.setCursor(x, FREQUENCY_Y);
  tft.print(freqBuffer);

  // show inactive freq if requested
  if(includeInactiveVFO) {
    x = t41.ActiveVFO == VFO_A ? VFO_B_INACTIVE_OFFSET : FREQUENCY_X + 20;
    //tft.setFontScale(1, 2);
    tft.setFont(Arial_16);
    tft.fillRect(x, FREQUENCY_Y, 15 * 8, 20*8, RA8875_BLACK);

    tft.setTextColor(ST7735_GREEN);
    FormatFrequency(t41.InactiveFreq, freqBuffer);
    tft.setCursor(x, FREQUENCY_Y);
    tft.print(freqBuffer);
  }
}

/*****
  Purpose: To display the current transmission frequency, band, mode, and sideband above the spectrum display
*****/
FLASHMEM void ShowOperatingStats() {
  //tft.setFontScale((enum RA8875tsize)0);
  tft.setFont(Arial_8);

  // clear operating stats
  //tft.fillRect(OPERATION_STATS_L, OPERATION_STATS_T, OPERATION_STATS_W, tft.getFontHeight(), ST7735_BLACK);
  tft.fillRect(OPERATION_STATS_L, OPERATION_STATS_T, OPERATION_STATS_W, 10, ST7735_BLACK);

  // print center frequency
  tft.setCursor(OPERATION_STATS_L, OPERATION_STATS_T);
  tft.setTextColor(ST7735_WHITE);
  tft.print("CF");
  tft.setCursor(OPERATION_STATS_CF, OPERATION_STATS_T);
  //tft.setTextColor(ST7735_ORANGE);
  tft.setTextColor(ST7735_RED);
  if(t41.SpectrumZoom == 0) {
    tft.print(t41.CenterFreq + (long)t41.IntermediateFreq);
  } else {
    tft.print(t41.CenterFreq);
  }

  // print band for the active VFO
  //tft.setTextColor(ST7735_ORANGE);
  tft.setTextColor(ST7735_RED);
  tft.setCursor(OPERATION_STATS_BD, OPERATION_STATS_T);
  tft.print(bands[t41.ActiveBand].name);

  tft.setTextColor(ST7735_GREEN);
  tft.setCursor(OPERATION_STATS_MD, OPERATION_STATS_T);

  switch(t41.RadioMode) {
    case SSB_MODE:
      tft.print("SSB");
      break;

    case CW_MODE:
      tft.print("CW ");
      tft.setCursor(OPERATION_STATS_CWF, OPERATION_STATS_T);
      tft.print(menuOptions[1][t41.CWFilterIndex]);
      break;

    case DSB_MODE:
      tft.print("DSB");
      break;

    case DATA_MODE:
      tft.print("DATA");
      break;
  }

  tft.setCursor(OPERATION_STATS_DMD, OPERATION_STATS_T);
  tft.setTextColor(ST7735_WHITE);

  tft.print(DEMOD[t41.DemodMode].text);

  ShowCurrentPowerSetting();
}

/*****
  Purpose: Display current power setting
*****/
FLASHMEM void ShowCurrentPowerSetting() {
  //tft.setFontScale((enum RA8875tsize)0);
  tft.setFont(Arial_8);

  tft.fillRect(OPERATION_STATS_PWR, OPERATION_STATS_T, 6 * 11, 8, ST7735_BLACK);
  tft.setCursor(OPERATION_STATS_PWR, OPERATION_STATS_T);
  tft.setTextColor(ST7735_RED);
  if(t41.TxPower < 15) {
    tft.setTextColor(ST7735_GREEN);
  } else {
    tft.setTextColor(ST7735_RED);
  }
  tft.print(t41.TxPower, 1);  // Power output is a float
  if(t41.TxPower == 1) {
    tft.print(" Watt");
  } else {
    tft.print(" Watts");
  }
}

/*****
  Purpose: draws static display items
*****/
FLASHMEM void DrawStaticDisplayItems() {
  DrawSpectrumFrame();
  //DrawSMeterContainer();
  //DrawAudioSpectContainer();
  //ClearInfoBox();
}
