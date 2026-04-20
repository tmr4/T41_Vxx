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

#define MAX_FREQ_INDEX  8

bool lowerAudioFilterActive = false; // false - upper, true - lower audio filter active
int liveNoiseFloorFlag = OFF;         // ON=1, OFF=0, Auto=-1

bool nfmBWFilterActive = false; // false - audio filters active, true - NFM BW demod filter active

//------------------------- Local Variables ----------
bool save_last_frequency = false;
bool directFreqFlag = false;
int TxRxFreqOld;

//-------------------------------------------------------------------------------------------------------------
// Forwards
//-------------------------------------------------------------------------------------------------------------

int ValidateDemodMode(int demod);

//-------------------------------------------------------------------------------------------------------------
// Code
//-------------------------------------------------------------------------------------------------------------

/*****
  Purpose: To process a band increase/decrease

  *** radio mode and DSB/data demolation mode are unchanged across band changes ***
*****/
FLASHMEM void ChangeBand(int change) {
  int TxRxFreq;

  // Added if so unused GPOs will not be touched
  if(currentBand < BAND_12M) {
    digitalWrite(bandswitchPins[currentBand], LOW);
  }

  currentBand += change;
  if(currentBand == NUMBER_OF_BANDS) {  // Incremented too far?
    currentBand = 0;                     // Yep. Roll to list front.
  }
  if(currentBand < 0) {                 // Incremented too far?
    currentBand = NUMBER_OF_BANDS - 1;  // Yep. Roll to list front.
  }

  // *** DSB/data demolation mode are unchanged across band changes ***
  if((radioMode == SSB_MODE) || (radioMode == CW_MODE)) {
    currentDemodMode = ValidateDemodMode(-1);
  }

  t41.NCOFreq = 0L;
  TxRxFreq = t41.TXRXFreq();

  switch(activeVFO) {
    case VFO_A:
      if(save_last_frequency) {
        lastFrequencies[currentBandA][VFO_A] = TxRxFreq;
      } else {
        if(directFreqFlag) {
          lastFrequencies[currentBandA][VFO_A] = TxRxFreqOld;
          directFreqFlag = false;
        } else {
          lastFrequencies[currentBandA][VFO_A] = TxRxFreq;
        }
        TxRxFreqOld = TxRxFreq;
      }
      currentBandA = currentBand;
      t41.CenterFreq = currentFreqA = lastFrequencies[currentBandA][VFO_A];
      break;

    case VFO_B:
      if(save_last_frequency) {
        lastFrequencies[currentBandB][VFO_B] = TxRxFreq;
      } else {
        if(directFreqFlag) {
          lastFrequencies[currentBandB][VFO_B] = TxRxFreqOld;
          directFreqFlag = false;
        } else {
          lastFrequencies[currentBandB][VFO_B] = TxRxFreq;
        }
        TxRxFreqOld = TxRxFreq;
      }
      currentBandB = currentBand;
      t41.CenterFreq = currentFreqB = lastFrequencies[currentBandB][VFO_B];
      break;

    case VFO_SPLIT:
      DoSplitVFO();
      break;
  }

  // save band info if not calibrating
  // *** TODO: calibrate check from v12, validate for v11 calibration routines
  if(calibrateItem != 1) {
    EEPROMWrite();
  }

  if(radioMode == DATA_MODE) {
    switch(currentDemodMode) {
      case DEMOD_FT8:
        break;

      case DEMOD_FT8_INTERNAL:
      case DEMOD_FT8_WAV:
        // turn on FT8
        ft8SyncState = 0;
        ft8SyncState = 0; // not sync'd
        UpdateInfoBoxItem(IB_ITEM_FT8);
        infoBoxItemActive[IB_ITEM_FT8] = true;
        break;

      case DEMOD_PSK31:
      case DEMOD_PSK31_WAV:
        break;
    }
  }

  SetBand(TxRxFreq);

  if(currentBand < BAND_12M) {
    digitalWrite(bandswitchPins[currentBand], HIGH);
  }
}

/*****
  Purpose: Make a band change if needed due to a frequency change
*****/
FLASHMEM void ChangeBand(long newFreq) {
  int newBand = BAND_80M;

  // determine appropriate band for newFreq
  for(; newBand < NUMBER_OF_BANDS; newBand++) {
    if(newFreq <= bands[newBand].fBandHigh) {
      break;
    }
  }

  // change bands if newBand is valid and different than current band
  if((newBand < NUMBER_OF_BANDS) && (newBand != currentBand)) {
    ChangeBand(newBand - currentBand);
  }
}

/*****
  Purpose: Toggle which filter is adjusted by filter encoder
*****/
FLASHMEM void ButtonFilter() {
  switch(currentDemodMode) {
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

FLASHMEM void UpdateModeDisplay() {
  SetupDemodFilterBW();

  switch(displayState) {
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

// validate demod mode depending on the radio mode
//  returns default demod mode if demod = -1
FLASHMEM int ValidateDemodMode(int demod) {
  switch(radioMode) {
    case SSB_MODE:
    case CW_MODE:
      if(demod < 0) {
        demod = bands[currentBand].demod;
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
FLASHMEM void ChangeDemodMode(int demod) {
  int mode;

  if(demod == currentDemodMode) {
    return; // nothing to do
  }

  // change demod mode
  mode = ValidateDemodMode(demod);
  switch(radioMode) {
    case SSB_MODE:
    case CW_MODE:
    case DSB_MODE:
      currentDemodMode = mode;
      break;

    case DATA_MODE:
      ChangeMode(DATA_MODE, mode);
      return;
      break;

    default:
      break;
  }

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
  ChangeDemodMode(currentDemodMode + 1);
}

/*****
  Purpose: Sets radio operating mode
    mode:  desired radio mode
    demod: desired demod mode (-1 give default demod mode)
    *** current demod mode is not preserved over mode changes ***
*****/
FLASHMEM void ChangeMode(int mode, int demod /* = -1 */) {
  if(mode == radioMode) {
    if((demod < 0) || (demod == currentDemodMode)) {
      return; // nothing to do
    } // else continue to change demod mode for current radio mode
  }

  // ignore invalid modes
  if((mode < SSB_MODE) || (mode > DATA_MODE)) {
    return; // nothing to do
  }

  // switching modes, wrap up current mode
  switch(radioMode) {
    case CW_MODE:
      // hide cw related items in info box
      infoBoxItemActive[IB_ITEM_DECODER] = false;
      infoBoxItemActive[IB_ITEM_KEY] = false;
      UpdateInfoBoxItem(IB_ITEM_DECODER);
      UpdateInfoBoxItem(IB_ITEM_KEY);

      if(decoderFlag == ON) {
        ExitCWDecoder();
      }

      // turn off keyer
      keyerState = 0;
      infoBoxItemActive[IB_ITEM_KEYER] = false;
      ClearInfoBoxKeyer();

      // *** TODO: do this early here, otherwise this won't erase filter if on as radio mode hasn't changed yet ***
      radioMode = mode;
      DrawCWFilter();
      break;

    case DATA_MODE:
      switch(currentDemodMode) {
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

  // set new radio mode and demod mode
  radioMode = mode;
  currentDemodMode = ValidateDemodMode(demod);

  // set up radio for changes
  switch(radioMode) {
    case CW_MODE:
      // show cw related items in info box
      infoBoxItemActive[IB_ITEM_DECODER] = true;
      infoBoxItemActive[IB_ITEM_KEY] = true;
      UpdateInfoBoxItem(IB_ITEM_DECODER);
      UpdateInfoBoxItem(IB_ITEM_KEY);

      if(decoderFlag == ON) {
        // init if decoding CW
        InitCWDecoder();

        DrawCWFilter();
      }

      // turn on keyer
      keyerState = 1;
      infoBoxItemActive[IB_ITEM_KEYER] = true;
      UpdateInfoBoxItem(IB_ITEM_KEYER);
      break;

    case DATA_MODE:
      switch(currentDemodMode) {
        case DEMOD_FT8:
          InitFT8();
          break;

        case DEMOD_FT8_INTERNAL:
          // try to set up internal FT8 ops
          // *** TODO: make generic from config file ***
          if(!InitFT8Decoder("KN6ZDE", "CM87")) {
            // can't set up FT8 decode, fall back to normal FT8
            ExitFT8Decoder();

            currentDemodMode = DEMOD_FT8;
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
              UpdateInfoBoxItem(IB_ITEM_FT8);
            } else {
              // couldn't load wav file
              ExitFT8Decoder();

              // can't set up FT8 decode, fall back to normal FT8
              currentDemodMode = DEMOD_FT8;
              InitFT8();
            }
          } else {
            // can't set up FT8 decode, fall back to normal FT8
            ExitFT8Decoder();

            currentDemodMode = DEMOD_FT8;
            InitFT8();
          }
          break;

        case DEMOD_PSK31:
          //  *** TODO: psk31 doesn't work right now, problem processing signal in ProcessReceiverData with new yield process ***
          //setupPSK31();
          //currentDemodMode = DEMOD_PSK31;
          break;

        default:
          break;
      }
      break;
  }

  UpdateModeDisplay();
}

/*****
  Purpose: change to the next standard mode, SSB -> CW -> DSB -> Data -> SSB
*****/
FLASHMEM void ButtonMode() {
  int mode = radioMode + 1;

  if((mode > DATA_MODE) || (mode < SSB_MODE)) {
    mode = SSB_MODE;
  }

  ChangeMode(mode);
}

/*****
  Purpose: To process select noise reduction
*****/
FLASHMEM void ButtonNR() {
  nrOptionSelect++;
  if(nrOptionSelect > NR_OPTIONS) {
    nrOptionSelect = 0;
  }

  UpdateInfoBoxItem(IB_ITEM_FILTER);
}

/*****
  Purpose: To set the notch filter
*****/
FLASHMEM void ButtonNotchFilter() {
  ANR_notchOn = !ANR_notchOn;
  delay(100L);
}


/*****
  Purpose:  Toggles flag to allow quick setting of noise floor in spectrum display.
            Saves current noise floor to EEPROM when toggled to Off.  A band's
            current noise floor isn't preserved in EEPROM if you switch bands while
            toggle is On.
*****/
FLASHMEM void ToggleLiveNoiseFloorFlag() {
  // save final noise floor setting if toggling from ON
  if(liveNoiseFloorFlag == 2) {
    //EEPROMData.currentNoiseFloor[currentBand]  = currentNoiseFloor[currentBand];
    EEPROMWrite();
  }

  // toggle noise floor flag: OFF -> Auto -> ON -> OFF
  liveNoiseFloorFlag += 1;
  if(liveNoiseFloorFlag > 2) liveNoiseFloorFlag = 0;
  UpdateInfoBoxItem(IB_ITEM_FLOOR);
}

/*****
  Purpose: To process a frequency increment button push
*****/
FLASHMEM void ChangeFreqIncrement(int change) {
  long incrementValues[] = { 10, 50, 100, 250, 1000, 10000, 100000, 1000000 };

  tuneIndex += change;
  if(tuneIndex < 0) {
    tuneIndex = MAX_FREQ_INDEX - 1;
  }
  if(tuneIndex >= MAX_FREQ_INDEX) {
    tuneIndex = 0;
  }

  freqIncrement = incrementValues[tuneIndex];

  UpdateInfoBoxItem(IB_ITEM_TUNE);
}

/*****
  Purpose: To process a fine tune frequency increment button push
*****/
FLASHMEM void ChangeFtIncrement(int change) {
  long selectFT[] = { 10, 50, 250, 500 };

  ftIndex += change;
  if(ftIndex > 3) {
    ftIndex = 0;
  }
  if(ftIndex < 0) {
    ftIndex = 3;
  }

  ftIncrement = selectFT[ftIndex];

  UpdateInfoBoxItem(IB_ITEM_FINE);
}

/*****
  Purpose: To process a fine tune frequency increment button push
*****/
FLASHMEM void ToggleCWDecoder() {
  decoderFlag = !decoderFlag;
  UpdateInfoBoxItem(IB_ITEM_DECODER);

  if(radioMode == CW_MODE) {
    if(decoderFlag == ON) {
      InitCWDecoder();
    } else {
      ExitCWDecoder();
    }

    DrawCWFilter();
  }
}
