
#include "SDT.h"

#include "Display.h"
#include "Encoders.h"
#include "t41Control.h"
#include "Tune.h"

/*

This class replaces many old T41 global variables with properties and helper functions.
  Properties: provide notification of change events
  Helper: simple function returning a value

Properties that replaced old global variables:
  RemoteStatus

  // the following properties provide remote notifications and display updates
    CenterFreq
    NCOFreq
    AudioVolume
    FilterHiCut
    FilterLoCut

  // the following properties don't provide any notifications
    // these are mainly used to keep track of the VFO frequencies
    ActiveVFO
    ActiveBand;
    InactiveFreq;
    InactiveBand;

  Helper functions that do the job of old global variables:
    ActiveFreq



*/

//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

T41Properties t41;

//-------------------------------------------------------------------------------------------------------------
// Forwards
//-------------------------------------------------------------------------------------------------------------

//-------------------------------------------------------------------------------------------------------------
// Code
//-------------------------------------------------------------------------------------------------------------

template<typename T>
void NotifyPropertyChanged(T val) {
  Serial.print("Property changed to: "); Serial.println(val);
}

//T41Properties* T41Properties::instance = NULL;

T41Properties::T41Properties() {
  //if(instance == NULL) {
  //  instance = this;
  //}
  begin();
}

void T41Properties::begin() {
  // initialize properties
  SetPropertyDefaults();
}

void T41Properties::SetPropertyDefaults() {
  int remoteStatus = CAT_CONTROL_HOST || CAT_CONTROL ? REMOTE_NOT_CONNECTED : REMOTE_NOT_AVAIL;

  // polled properties
  RemoteStatus.Init(remoteStatus, &ShowRemoteStatus); // notify on change, not polled

  RadioMode.Init(SSB_MODE, &SendSetMode, &UpdateModeDisplay);
  DemodMode.Init(DEMOD_LSB, &SendSetDemodMode, &UpdateModeDisplay);
  ActiveBand.Init(BAND_40M, 0, NUMBER_OF_BANDS - 1, true, &SendBand, &UpdateDisplayBand);

  CenterFreq.Init(CURRENT_FREQ_A, &SendCenterFreq, &UpdateDisplayFreq);
  NCOFreq.Init(0, &CheckNCOFreqBounds, &SendNCOFreq, &UpdateDisplayNCOFreq);
  FilterHiCut.Init(3000, &SendFilterHi, &UpdateDisplayFilters);
  FilterLoCut.Init(200, &SendFilterLo, &UpdateDisplayFilters);

  AudioVolume.Init(30, MIN_AUDIO_VOLUME, MAX_AUDIO_VOLUME, false, &SendVolume, &UpdateInfoBoxItem, IB_ITEM_VOL);

  // *** TODO: these need notifications/updates added ***
  ActiveVFO.Init(VFO_A);

  // properties w/o notifications or display updates
  InactiveFreq.Init(CURRENT_FREQ_B);
  InactiveBand.Init(BAND_40M);
}

// helper functions
void T41Properties::Poll(bool updateDisplay, bool updateRemote) {
  // *** TODO: consider order to minimize update duplication ***
  // *** TODO: consider refining updates as there is some duplication ***
  RadioMode.Poll(updateDisplay, updateRemote);
  DemodMode.Poll(updateDisplay, updateRemote);
  ActiveBand.Poll(updateDisplay, updateRemote);
  CenterFreq.Poll(updateDisplay, updateRemote);
  NCOFreq.Poll(updateDisplay, updateRemote);
  FilterHiCut.Poll(updateDisplay, updateRemote);
  FilterHiCut.Poll(updateDisplay, updateRemote);
}

void T41Properties::PollInfoBox(bool updateDisplay, bool updateRemote) {
  AudioVolume.Poll(updateDisplay, updateRemote);
}

// these don't change NCOFreq
void T41Properties::SetFreqA(int f) {
  if(ActiveVFO == VFO_A) {
    SetCenterTune(f - CenterFreq);
  } else {
    InactiveFreq = f;
  }
}

void T41Properties::SetFreqB(int f) {
  if(ActiveVFO == VFO_B) {
    SetCenterTune(f - CenterFreq);
  } else {
    InactiveFreq = f;
  }
}

void T41Properties::SwapActiveVFO() {
  int tmp = ActiveBand;

  ActiveBand = InactiveBand;
  InactiveBand = tmp;

  t41.NCOFreq = 0L;
  tmp = CenterFreq;
  CenterFreq = InactiveFreq;
  InactiveFreq = tmp;
}

/*
Notes:

Track memory usage and loop timing as T41 properties are added:
  *** loop times are a rough average over 20 loops ***
  *** size on Audio Platform differs from PS due to USB Host ... examine ***

Memory Usage and Loop Timing on Teensy 4.1:
Project System:
  Compiler Settings: smallest code, 528MHz, Serial+MIDI+Audio
  Input: T41 vPS IQ waveforms, NF: Auto
  Timing: T41 timing profile

4/22/2026
Added hi/lo filter properties
  FLASH: code:206500, data:78244, headers:9136   free for files:7832584
   RAM1: variables:147040, code:170792, padding:25816   free for local variables:180640
   RAM2: variables:334048  free for malloc/new:190240
 EXTRAM: variables:1200320

4/21/2026
Working volume property callbacks and remote status
Set T41_USB_AUDIO to false, excluding some code and data though still compiling w/ Serial+MIDI+Audio

4/20/2026
ActiveFreq eliminated (8 byte FLASH code reduction only! obviously the compiler already optimized this away)
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
