#include <Audio.h>

#include "SDT.h"

#include "Button.h"
#include "ButtonProc.h"
#include "CWProcessing.h"
#include "Display.h"
#include "EEPROM.h"
#include "Encoders.h"
#include "Filter.h"
#include "ft8.h"
#include "keyer.h"
#include "Menu.h"
#include "MenuProc.h"
#include "Noise.h"
#include "Process.h"
#include "psk31.h"
#include "Tune.h"
#include "Utility.h"

//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

bool lowerAudioFilterActive = false; // false - upper, true - lower audio filter active

bool nfmBWFilterActive = false; // false - audio filters active, true - NFM BW demod filter active

int bandNoiseFloor[NUMBER_OF_BANDS] = { 0, 0, 0, 0, 0, 0, 0 };

//-------------------------------------------------------------------------------------------------------------
// Forwards
//-------------------------------------------------------------------------------------------------------------

int ValidateDemodMode(int demod);

//-------------------------------------------------------------------------------------------------------------
// Code
//-------------------------------------------------------------------------------------------------------------

// process band change
// *** FilterHiCut and FilterLoCut will notify if changed ***
FLASHMEM void ChangeBand(int change, bool notify /* = true */) {
  int vfo = t41.ActiveVFO;
  int from = t41.ActiveBand;

  if(from < BAND_12M) {
    digitalWrite(bandswitchPins[from], LOW);
  }

  // *** SSB/data demolation mode are unchanged across band changes ***
  if((t41.RadioMode == SSB_MODE) || (t41.RadioMode == CW_MODE)) {
    t41.DemodMode = ValidateDemodMode(-1);
  }

  if(vfo == VFO_SPLIT) {
    DoSplitVFO();
  } else {
    lastFrequencies[from][vfo] = t41.ActiveFreq();
    t41.CenterFreq.Update(lastFrequencies[t41.ActiveBand][vfo]);
  }
  t41.NCOFreq.Update(0);

  if(notify) {
    t41.ActiveBand += change;
  } else {
    t41.ActiveBand.Update(from + change);
  }

  // save band info if not calibrating
  // *** TODO: calibrate check from v12, validate for v11 calibration routines
  if(calibrateItem != 1) {
    EEPROMWrite();
  }

  if(t41.RadioMode == DATA_MODE) {
    switch(t41.DemodMode) {
      case DEMOD_FT8:
        // limit zoom in FT8 mode to 2x and 4x
        if((t41.SpectrumZoom == 0) || (t41.SpectrumZoom > 2)) {
          t41.SpectrumZoom = 1;
        }
        break;

      case DEMOD_FT8_INTERNAL:
      case DEMOD_FT8_WAV:
        // turn on FT8
        ft8SyncState = 0;
        ft8SyncState = 0; // not sync'd
        UpdateInfoBoxItem(T41_ITEM_FT8);
        infoBoxItemActive[T41_ITEM_FT8] = true;
        break;

      case DEMOD_PSK31:
      case DEMOD_PSK31_WAV:
        break;
    }
  }

  SetupBandFreq(t41.CenterFreq);

  if(t41.ActiveBand < BAND_12M) {
    digitalWrite(bandswitchPins[t41.ActiveBand], HIGH);
  }
}

/*
// make a band change if needed due to a frequency change
FLASHMEM void ChangeBand(long newFreq) {
  int newBand = BAND_80M;

  // determine appropriate band for newFreq
  for(; newBand < NUMBER_OF_BANDS; newBand++) {
    if(newFreq <= bands[newBand].fBandHigh) {
      break;
    }
  }

  // change bands if newBand is valid and different than current band
  if((newBand < NUMBER_OF_BANDS) && (newBand != t41.ActiveBand)) {
    ChangeBand(newBand - t41.ActiveBand);
  }
}
*/

/*****
  Purpose: Toggle which filter is adjusted by filter encoder
*****/
FLASHMEM void ButtonFilter() {
  switch(t41.DemodMode) {
    case DEMOD_NFM:
    // Active filter in NFM demod mode:
    // At startup:  high audio
    // 1st press:   NFM BW
    // 2nd press:   low audio
    // 3rd press:   high audio
    // repeat @ 1
    if(nfmBWFilterActive) {
      nfmBWFilterActive = !nfmBWFilterActive;
      lowerAudioFilterActive = !lowerAudioFilterActive;
    } else {
      if(lowerAudioFilterActive) {
        lowerAudioFilterActive = !lowerAudioFilterActive;
      } else {
        nfmBWFilterActive = !nfmBWFilterActive;
      }
    }
    break;

  default:
    lowerAudioFilterActive = !lowerAudioFilterActive;
    break;
  }

  ShowBandwidthBarValues(); // change color of active filter value
  DrawAudioFilterLines();
}

// validate demod mode depending on the radio mode
//  returns default demod mode if demod = -1
FLASHMEM int ValidateDemodMode(int demod) {
  switch(t41.RadioMode) {
    case SSB_MODE:
    case CW_MODE:
      if(demod < 0) {
        demod = bands[t41.ActiveBand].demod;
      } else if((demod > DEMOD_LSB) || (demod < DEMOD_USB)) {
        demod = DEMOD_USB;
      }
      break;

    case DSB_MODE:
      // unless specified DSB mode always starts in AM
      if((demod < 0) || (demod > DEMOD_NFM) || (demod < DEMOD_AM)) {
        demod = DEMOD_AM;
      }
      break;

    case DATA_MODE:
      // unless specified data mode always starts in FT8
      if((demod < 0) || (demod > DEMOD_FT8_WAV) || (demod < DEMOD_FT8)) {
        demod = DEMOD_FT8;
      }
      break;
  }

  return demod;
}

/*****
  Purpose: Change the demodulation mode
          *** can't use this to change radio mode ***
*****/
FLASHMEM void ChangeDemodMode(int demod, bool notify /* = true */) {
  int mode;

  if(demod == t41.DemodMode) {
    return; // nothing to do
  }

  // change demod mode
  mode = ValidateDemodMode(demod);
  switch(t41.RadioMode) {
    case SSB_MODE:
    case CW_MODE:
    case DSB_MODE:
      if(notify) {
        t41.DemodMode = mode;
      } else {
        t41.DemodMode.Update(mode);
      }
      break;

    case DATA_MODE:
      ChangeMode(DATA_MODE, mode, notify);
      return;
      break;

    default:
      break;
  }

  SetupDemodFilterBW();
  UpdateModeDisplay();
}

/*****
  Purpose: change to the next standard demod mode for radio mode
      SSB:  USB <-> LSB
      CW:   USB <-> LSB
      DSB:  AM -> SAM -> FM -> AM (receive only)
      DATA: FT8 -> FT8.int -> FT8.wav
*****/
FLASHMEM void ButtonDemodMode() {
  ChangeDemodMode(t41.DemodMode + 1);
}

/*****
  Purpose: Sets radio operating mode
    mode:  desired radio mode
    demod: desired demod mode (-1 give default demod mode)
    *** current demod mode is not preserved over mode changes ***
*****/
FLASHMEM void ChangeMode(int mode, int demod /* = -1 */, bool notify /* = true */) {
  int tmp = t41.RadioMode;

  if(mode == t41.RadioMode) {
    if((demod < 0) || (demod == t41.DemodMode)) {
      return; // nothing to do
    } // else continue to change demod mode for current radio mode
  }

  // ignore invalid modes
  if((mode < SSB_MODE) || (mode > DATA_MODE)) {
    return; // nothing to do
  }

  if(notify) {
    t41.RadioMode = mode;
  } else {
    t41.RadioMode.Update(mode);
  }

  // switching modes, wrap up current mode
  switch(tmp) {
    case CW_MODE:
      // hide cw related items in info box
      infoBoxItemActive[T41_ITEM_DECODER] = false;
      infoBoxItemActive[T41_ITEM_KEY] = false;
      UpdateInfoBoxItem(T41_ITEM_DECODER);
      UpdateInfoBoxItem(T41_ITEM_KEY);

      if(decoderFlag == ON) {
        ExitCWDecoder();
      }

      // turn off keyer
      keyerState = 0;
      infoBoxItemActive[T41_ITEM_KEYER] = false;
      ClearInfoBoxKeyer();

      DrawCWFilter();
      break;

    case DATA_MODE:
      switch(t41.DemodMode) {
        case DEMOD_FT8:
          ExitFT8();
          break;

        case DEMOD_FT8_INTERNAL:
        case DEMOD_FT8_WAV:
          ExitFT8Decoder();
          break;

        case DEMOD_PSK31:
        case DEMOD_PSK31_WAV:
          //exitPSK31();
          break;

        default:
          break;
      }
      break;

    default:
      break;
  }

  // set new demod mode
  t41.DemodMode = ValidateDemodMode(demod);

  // set up radio for changes
  switch(t41.RadioMode) {
    case CW_MODE:
      // show cw related items in info box
      infoBoxItemActive[T41_ITEM_DECODER] = true;
      infoBoxItemActive[T41_ITEM_KEY] = true;
      UpdateInfoBoxItem(T41_ITEM_DECODER);
      UpdateInfoBoxItem(T41_ITEM_KEY);

      if(decoderFlag == ON) {
        // init if decoding CW
        InitCWDecoder();

        DrawCWFilter();
      }

      // turn on keyer
      keyerState = 1;
      infoBoxItemActive[T41_ITEM_KEYER] = true;
      UpdateInfoBoxItem(T41_ITEM_KEYER);
      break;

    case DATA_MODE:
      switch(t41.DemodMode) {
        case DEMOD_FT8:
          InitFT8();
          break;

        case DEMOD_FT8_INTERNAL:
          // try to set up internal FT8 ops
          // *** TODO: make generic from config file ***
          if(!InitFT8Decoder("KN6ZDE", "CM87")) {
            // can't set up FT8 decode, fall back to normal FT8
            ExitFT8Decoder();

            t41.DemodMode = DEMOD_FT8;
            InitFT8();
          }
          break;

        case DEMOD_FT8_WAV:
          if(InitFT8Decoder("KN6ZDE", "CM87")) {
            // try to load wav file
            if(SetupFT8Wav()) {
              // switch to play a wav file
              ft8SyncState = 1;
              ft8SyncState = 1;
              UpdateInfoBoxItem(T41_ITEM_FT8);
            } else {
              // couldn't load wav file
              ExitFT8Decoder();

              // can't set up FT8 decode, fall back to normal FT8
              t41.DemodMode = DEMOD_FT8;
              InitFT8();
            }
          } else {
            // can't set up FT8 decode, fall back to normal FT8
            ExitFT8Decoder();

            t41.DemodMode = DEMOD_FT8;
            InitFT8();
          }
          break;

        case DEMOD_PSK31:
          //  *** TODO: psk31 doesn't work right now, problem processing signal in ProcessReceiverData with new yield process ***
          //setupPSK31();
          //t41.DemodMode = DEMOD_PSK31;
          break;

        default:
          break;
      }
      break;
  }

  SetupDemodFilterBW();
}

/*****
  Purpose: change to the next standard mode, SSB -> CW -> DSB -> Data -> SSB
*****/
FLASHMEM void ButtonMode() {
  int mode = t41.RadioMode + 1;

  if((mode > DATA_MODE) || (mode < SSB_MODE)) {
    mode = SSB_MODE;
  }

  ChangeMode(mode);
}

/*****
  Purpose: To set the notch filter
*****/
FLASHMEM void ButtonNotchFilter() {
  ANR_notchOn = !ANR_notchOn;
  delay(100L);
}


/*****
  Purpose: To process a frequency increment button push
*****/
FLASHMEM void ChangeFreqIncrement(int change, bool notify /* = true */) {
  int index = t41.CenterTuneIndex + change;

  if(notify) {
    t41.CenterTuneIndex = index;
  } else {
    t41.CenterTuneIndex.Update(index);
  }
}

/*****
  Purpose: To process a fine tune frequency increment button push
*****/
FLASHMEM void ChangeFtIncrement(int change, bool notify /* = true */) {
  int index = t41.FineTuneIndex + change;

  if(notify) {
    t41.FineTuneIndex = index;
  } else {
    t41.FineTuneIndex.Update(index);
  }
}

/*****
  Purpose: To process a fine tune frequency increment button push
*****/
FLASHMEM void ToggleCWDecoder() {
  decoderFlag = !decoderFlag;
  UpdateInfoBoxItem(T41_ITEM_DECODER);

  if(t41.RadioMode == CW_MODE) {
    if(decoderFlag == ON) {
      InitCWDecoder();
    } else {
      ExitCWDecoder();
    }

    DrawCWFilter();
  }
}
