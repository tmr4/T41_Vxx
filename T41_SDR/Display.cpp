
#include "SDT.h"

#include "Display.h"
#include "MenuProc.h"
#include "Process.h"

#include "debug.h"

// consolidates calls to various display functions to update various portions of the T41 display

//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

extern int calibrateItem;

//-------------------------------------------------------------------------------------------------------------
// Forwards
//-------------------------------------------------------------------------------------------------------------

//-------------------------------------------------------------------------------------------------------------
// Code
//-------------------------------------------------------------------------------------------------------------

FLASHMEM void UpdateModeDisplay() {
  switch(t41.DisplayState) {
    case DISPLAY_T41:
      ShowOperatingStats();
      ShowBandwidthBarValues();
      DrawBandwidthBar();
      DrawAudioSpectContainer();
      DrawAudioFilterLines();
      break;

    case DISPLAY_T41_FT8_DECODE:
      ShowOperatingStats();
      DrawAudioSpectContainer();
      DrawAudioFilterLines();
      break;

    case DISPLAY_BEACON_MONITOR:
      break;

    default:
    // no screen updates at all
    break;
  }

  // *** TODO: where is this shown? Add to info box for v12 ***
  //ShowAnalogGain();
}

// updates various display elements associated with tuning frequency
FLASHMEM void UpdateDisplayFreq() {
  switch(t41.DisplayState) {
    case DISPLAY_T41:
      ShowFrequency();          // update frequency display
      ShowOperatingStats();     // update center frequency in band info
      ShowSpectrumFreqValues(); // update spectrum frequency values
      break;

    case DISPLAY_BEACON_MONITOR:
      break;

    case DISPLAY_CALIBRATION:
      ShowOperatingStats();

      if(calibrateItem == 1) {
        // receive IQ calibration
        ShowSpectrumFreqValues();
      }
      break;

    case DISPLAY_FULL_MENU:
      ShowFrequency();
      ShowOperatingStats();
      break;

    default:
    // no screen updates at all
    break;
  }
}

FLASHMEM void UpdateDisplayNCOFreq() {
  switch(t41.DisplayState) {
    case DISPLAY_T41:
      ShowFrequency();
      DrawBandwidthBar();
      ShowBandwidthBarValues();
      break;

    case DISPLAY_FULL_MENU:
      ShowFrequency();
      break;

    default:
    // no screen updates at all
    break;
  }
}

FLASHMEM void UpdateDisplayBand() {
  switch(t41.DisplayState) {
    case DISPLAY_T41:
      ShowFrequency();
      ShowOperatingStats();
      DrawBandwidthBar();
      ShowBandwidthBarValues();
      ShowSpectrumFreqValues();
      DrawAudioFilterLines();
      break;

    case DISPLAY_BEACON_MONITOR:
      break;

    case DISPLAY_T41_FT8_DECODE:
      ShowFrequency();
      ShowOperatingStats();
      DrawAudioFilterLines();
      break;

    case DISPLAY_FULL_MENU:
      ShowFrequency();
      ShowOperatingStats();
      DrawAudioFilterLines();
      break;

    default:
    // no screen updates at all
    break;
  }
}

FLASHMEM void UpdateDisplayFilters() {
  switch(t41.DisplayState) {
    case DISPLAY_T41:
      ShowBandwidthBarValues();
      DrawBandwidthBar();
      DrawAudioFilterLines();
      break;

    case DISPLAY_T41_FT8_DECODE:
      DrawAudioFilterLines();
      break;

    case DISPLAY_BEACON_MONITOR:
    case DISPLAY_FULL_MENU:
    default:
    // no screen updates at all
    break;
  }
}

FLASHMEM void UpdateDisplayZoom() {
  switch(t41.DisplayState) {
    case DISPLAY_T41:
      DrawBandwidthBar();
      ShowSpectrumFreqValues();
      ShowOperatingStats(); // needes for to or from 1x zoom
      break;

    default:
    // no screen updates at all
    break;
  }
}

FASTRUN void UpdateLiveDisplayAreas() {
  YieldToProcess(true);
  SETPROFILEPIN(PROFILER_DRAW);
  DrawFreqSpectrum();
  RESETPROFILEPIN(PROFILER_DRAW);
  if(t41.CalState == NOT_CAL_STATE) {
    DrawWaterfall();
    SETPROFILEPIN(PROFILER_DRAW);
    DrawAudioSpectrum();
    RESETPROFILEPIN(PROFILER_DRAW);
  }
  DrawSmeterBar();
}
