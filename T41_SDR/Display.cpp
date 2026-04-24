
#include "SDT.h"

#include "Display.h"
#include "MenuProc.h"

// consolidates calls to various display functions to update various portions of the T41 display

//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

//-------------------------------------------------------------------------------------------------------------
// Forwards
//-------------------------------------------------------------------------------------------------------------

//-------------------------------------------------------------------------------------------------------------
// Code
//-------------------------------------------------------------------------------------------------------------

// updates various display elements associated with tuning frequency
FLASHMEM void UpdateDisplayFreq() {
  switch(displayState) {
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
  switch(displayState) {
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
  switch(displayState) {
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
  switch(displayState) {
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
