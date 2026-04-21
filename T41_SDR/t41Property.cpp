
#include "SDT.h"

#include "Display.h"
#include "Encoders.h"
#include "t41Control.h"

/*

This class replaces many old T41 global variables with properties and helper functions.
  Properties: provide notification of change events
  Helper: simple function returning a value

Properties that replaced old global variables:
  CenterFreq
  AudioVolume
  NCOFreq


Helper functions that do the job of old global variables:
TXRXFreq



*/

/*

Track memory usage and loop timing as T41 properties are added:
  *** loop times are a rough average over 20 loops ***

Memory Usage and Loop Timing on Teensy 4.1:
Project System:
  Compiler Settings: smallest code, 528MHz, Serial+MIDI+Audio
  Input: T41 vPS IQ waveforms, NF: Auto
  Timing: T41 timing profile

4/20/2026
TXRXFreq eliminated (8 byte FLASH code reduction only! obviously the compiler already optimized this away)
  FLASH: code:206876, data:78244, headers:8760   free for files:7832584
   RAM1: variables:147104, code:171352, padding:25256   free for local variables:180576
   RAM2: variables:334048  free for malloc/new:190240
 EXTRAM: variables:1200320

NCOFreq made a property
  FLASH: code:206884, data:78244, headers:8752   free for files:7832584
   RAM1: variables:147104, code:171352, padding:25256   free for local variables:180576
   RAM2: variables:334048  free for malloc/new:190240
 EXTRAM: variables:1200320

With most Property functions assigned to FLASHMEM
  Loop time: 96ms
  FLASH: code:206804, data:78244, headers:8832   free for files:7832584
   RAM1: variables:147072, code:171304, padding:25304   free for local variables:180608
   RAM2: variables:334048  free for malloc/new:190240
 EXTRAM: variables:1200320

audioVolume completly purged
  FLASH: code:206804, data:78244, headers:8832   free for files:7832584
   RAM1: variables:147072, code:171304, padding:25304   free for local variables:180608
   RAM2: variables:334048  free for malloc/new:190240
 EXTRAM: variables:1200320

AudioVolume property incorporated in code
  FLASH: code:206412, data:78244, headers:8200   free for files:7833608
   RAM1: variables:147040, code:170888, padding:25720   free for local variables:180640
   RAM2: variables:334048  free for malloc/new:190240
 EXTRAM: variables:1200320

New Property template w/ added AudioVolume property but not incorporated
  Loop time: 96ms
  FLASH: code:206412, data:78244, headers:8200   free for files:7833608
   RAM1: variables:147040, code:170840, padding:25768   free for local variables:180640
   RAM2: variables:334048  free for malloc/new:190240
 EXTRAM: variables:1200320

4/19/2026
Remaining reference to CenterFreq set to property
  Loop time: 96ms
  FLASH: code:206420, data:78244, headers:8192   free for files:7833608
   RAM1: variables:147008, code:170840, padding:25768   free for local variables:180672
   RAM2: variables:334048  free for malloc/new:190240
 EXTRAM: variables:1200320

After eliminating EEPROMData variables which aren't needed with T41 properties
  FLASH: code:206372, data:78244, headers:8240   free for files:7833608
   RAM1: variables:147008, code:170808, padding:25800   free for local variables:180672
   RAM2: variables:334048  free for malloc/new:190240
 EXTRAM: variables:1200320

After adding CenterFreq and replacing most relevant reference to centerFreq
  Loop time: 96ms
  FLASH: code:224572, data:84388, headers:8472   free for files:7809032
   RAM1: variables:155232, code:178344, padding:18264   free for local variables:172448
   RAM2: variables:334048  free for malloc/new:190240
 EXTRAM: variables:1200320

Prior to adding T41 class
  Loop time: 96ms
  FLASH: code:224028, data:84388, headers:9016   free for files:7809032
   RAM1: variables:155200, code:177896, padding:18712   free for local variables:172480
   RAM2: variables:334048  free for malloc/new:190240
 EXTRAM: variables:1200320

*/

T41Properties t41;

template<typename T>
void NotifyPropertyChanged(T val) {
  Serial.print("Property changed to: "); Serial.println(val);
}

T41Properties::T41Properties() {
  begin();
}

void T41Properties::begin() {
  // initialize properties
  SetPropertyDefaults();
}

void T41Properties::SetPropertyDefaults() {
  int remoteMode = CAT_CONTROL_HOST || CAT_CONTROL ? REMOTE_NOT_CONNECTED : REMOTE_NOT_AVAIL;

  RemoteMode.Init(remoteMode);
  CenterFreq.Init(7074000);
  NCOFreq.Init(0);
  AudioVolume.Init(30, MIN_AUDIO_VOLUME, MAX_AUDIO_VOLUME, &SendVolume, &UpdateInfoBoxItem, IB_ITEM_VOL);

    /*
    AGCMode = EEPROMData.AGCMode;
    rfGainAllBands = EEPROMData.rfGainAllBands;
    spectrumNoiseFloor = EEPROMData.spectrumNoiseFloor;
    tuneIndex = EEPROMData.tuneIndex;
    ftIndex = EEPROMData.ftIndex;
    transmitPowerLevel = EEPROMData.transmitPowerLevel;
    radioMode = EEPROMData.radioMode;
    nrOptionSelect = EEPROMData.nrOptionSelect;
    currentScale = EEPROMData.currentScale;
    spectrumZoom = EEPROMData.spectrumZoom;
    spectrum_display_scale = EEPROMData.spectrum_display_scale;

    cwFilterIndex = EEPROMData.cwFilterIndex;
    paddleDit = EEPROMData.paddleDit;
    paddleDah = EEPROMData.paddleDah;
    decoderFlag = EEPROMData.decoderFlag;
    keyType = EEPROMData.keyType;
    currentWPM = EEPROMData.currentWPM;
    sidetoneVolume = EEPROMData.sidetoneVolume;
    cwTransmitDelay = (unsigned long) EEPROMData.cwTransmitDelay;

    activeVFO = EEPROMData.activeVFO;
    freqIncrement = EEPROMData.freqIncrement; // *** this isn't needed if tuneIndex is used to set initial value ***

    currentBand = EEPROMData.currentBand;
    currentBandA = EEPROMData.currentBandA;
    currentBandB = EEPROMData.currentBandB;
  //  currentFreqA = EEPROMData.lastFrequencies[currentBandA][0];
  //  currentFreqB = EEPROMData.lastFrequencies[currentBandB][1];
    currentFreqA = EEPROMData.currentFreqA;
    currentFreqB = EEPROMData.currentFreqB;
    freqCorrectionFactor = EEPROMData.freqCorrectionFactor;

    for(int i = 0; i < EQUALIZER_CELL_COUNT; i++) {
      equalizerRec[i] = EEPROMData.equalizerRec[i];
      equalizerXmt[i] = EEPROMData.equalizerXmt[i];
    }

    currentMicThreshold = EEPROMData.currentMicThreshold;
    currentMicCompRatio = EEPROMData.currentMicCompRatio;
    currentMicAttack = EEPROMData.currentMicAttack;
    currentMicRelease = EEPROMData.currentMicRelease;
    currentMicGain = EEPROMData.currentMicGain;

    for(int i = 0; i < NUMBER_OF_SWITCHES; i++) {
      switchValues[0] = EEPROMData.switchValues[0];
    }

    LPFcoeff = EEPROMData.LPFcoeff;
    NR_PSI = EEPROMData.NR_PSI;
    NR_alpha = EEPROMData.NR_alpha;
    NR_beta = EEPROMData.NR_beta;
    omegaN = EEPROMData.omegaN;
    pll_fmax = EEPROMData.pll_fmax;

    for(int i = 0; i < NUMBER_OF_BANDS; i++) {
      powerOutCW[i] = EEPROMData.powerOutCW[i];
      powerOutSSB[i] = EEPROMData.powerOutSSB[i];
      CWPowerCalibrationFactor[i] = EEPROMData.CWPowerCalibrationFactor[i];
      SSBPowerCalibrationFactor[i] = EEPROMData.SSBPowerCalibrationFactor[i];
      IQAmpCorrectionFactor[i] = EEPROMData.IQAmpCorrectionFactor[i];
      IQPhaseCorrectionFactor[i] = EEPROMData.IQPhaseCorrectionFactor[i];
      IQXAmpCorrectionFactor[i] = EEPROMData.IQXAmpCorrectionFactor[i];
      IQXPhaseCorrectionFactor[i] = EEPROMData.IQXPhaseCorrectionFactor[i];
    }

    for(int i = 0; i < 13; i++) {
      favoriteFreqs[i] = EEPROMData.favoriteFreqs[i];
    }

    for(int i = 0; i < NUMBER_OF_BANDS; i++) {
      lastFrequencies[i][0] = EEPROMData.lastFrequencies[i][0];
      lastFrequencies[i][1] = EEPROMData.lastFrequencies[i][1];
    }

    strncpy(mapFileName, EEPROMData.mapFileName, 50);
    strncpy(myCall, EEPROMData.myCall, 10);
    strncpy(myTimeZone, EEPROMData.myTimeZone, 10);

    paddleFlip = EEPROMData.paddleFlip;
    sdCardPresent = EEPROMData.sdCardPresent;

    myLat = EEPROMData.myLat;
    myLong = EEPROMData.myLong;
    for(int i = 0; i < NUMBER_OF_BANDS; i++) {
      currentNoiseFloor[i] = EEPROMData.currentNoiseFloor[i];
    }
    compressorFlag = EEPROMData.compressorFlag;

    buttonThresholdPressed = EEPROMData.buttonThresholdPressed;
    buttonThresholdReleased = EEPROMData.buttonThresholdReleased;
    buttonRepeatDelay = EEPROMData.buttonRepeatDelay;
    */
}
