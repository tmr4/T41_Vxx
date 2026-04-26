
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

//#define MAX_FREQ_INDEX  8
#define MAX_ZOOM_ENTRIES      5

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

  // notify properties (not polled!)
  RemoteStatus.Init(remoteStatus, &ShowRemoteStatus);
  MouseCenterTuneActive.Init(false, &SendMouseCenterTuneActive, &HighlightTuneInc, false); // make it a notify property
  NoiseFloor.Init(0, &SendNoiseFloor, NULL, false); // make it a notify property

  // polled properties
  RadioMode.Init(SSB_MODE, &SendMode, &UpdateModeDisplay);
  DemodMode.Init(DEMOD_LSB, &SendDemodMode, &UpdateModeDisplay);
  ActiveBand.Init(BAND_40M, 0, NUMBER_OF_BANDS - 1, true, &SendBand, &UpdateDisplayBand);

  CenterFreq.Init(CURRENT_FREQ_A, &SendCenterFreq, &UpdateDisplayFreq);
  NCOFreq.Init(0, &CheckNCOFreqBounds, &SendNCOFreq, &UpdateDisplayNCOFreq);
  FilterHiCut.Init(3000, &SendFilterHi, &UpdateDisplayFilters);
  FilterLoCut.Init(200, &SendFilterLo, &UpdateDisplayFilters);

  // infobox properties
  AudioVolume.Init(30, MIN_AUDIO_VOLUME, MAX_AUDIO_VOLUME, false, &SendVolume, &UpdateInfoBoxItem, IB_ITEM_VOL);
  AGCMode.Init(1, 0, 5 - 1, true, &SendAGC, &UpdateInfoBoxItem, IB_ITEM_AGC);
  CenterTuneIndex.Init(DEFAULTFREQINDEX, 0, maxFreqIncIndex - 1, true, &SendFreqIncrement, &UpdateInfoBoxItem, IB_ITEM_TUNE);
  FineTuneIndex.Init(DEFAULT_FT_INDEX, 0, maxFtIncIndex - 1, true, &SendFtIncrement, &UpdateInfoBoxItem, IB_ITEM_FINE);
  SpectrumZoom.Init(1, 0, MAX_ZOOM_ENTRIES - 1, true, &SendDisplayZoom, &UpdateInfoBoxItem, IB_ITEM_ZOOM);
  LiveNoiseFloor.Init(0, 0, 2, true, &SendNFSetting, &UpdateInfoBoxItem, IB_ITEM_FLOOR); // OFF=0, Auto=1, ON=2

  // *** TODO: these need notifications/updates added ***
  ActiveVFO.Init(VFO_A);

  // properties w/o notifications or display updates
  InactiveFreq.Init(CURRENT_FREQ_B);
  InactiveBand.Init(BAND_40M);
}

// helper functions
void T41Properties::Poll(bool updateDisplay) {
  bool updateRemote = RemoteStatus == REMOTE_CONNECTED;

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

void T41Properties::PollInfoBox(bool updateDisplay) {
  bool updateRemote = RemoteStatus == REMOTE_CONNECTED;

  AudioVolume.Poll(updateDisplay, updateRemote);
  AGCMode.Poll(updateDisplay, updateRemote);
  CenterTuneIndex.Poll(updateDisplay, updateRemote);
  FineTuneIndex.Poll(updateDisplay, updateRemote);
  SpectrumZoom.Poll(updateDisplay, updateRemote);
  LiveNoiseFloor.Poll(updateDisplay, updateRemote);
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
Memory Usage and Loop Timing on Teensy 4.1:
Project System:
  Compiler Settings: smallest code, 528MHz, Serial+MIDI+Audio
  Input: T41 vPS IQ waveforms, NF: Auto
  Timing: T41 timing profile

*** loop times are a rough average over 20 loops ***
*** size on Audio Platform differs from PS due to mouse/keyboard support (and ? ... examine) ***

4/25/2026
PS
  FLASH: code:207052, data:78244, headers:8584   free for files:7832584
   RAM1: variables:147360, code:171512, padding:25096   free for local variables:180320
   RAM2: variables:334048  free for malloc/new:190240
 EXTRAM: variables:1200320
AP
  FLASH: code:215812, data:79268, headers:9040   free for files:7822344
   RAM1: variables:155456, code:180584, padding:16024   free for local variables:172224
   RAM2: variables:334304  free for malloc/new:189984
 EXTRAM: variables:480320

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
