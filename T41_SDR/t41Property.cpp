
#include "SDT.h"

#include "catControl.h"
#include "Display.h"
#include "Encoders.h"
#include "Noise.h"
//#include "t41Control.h"
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

void RFPowerFollowup();

//-------------------------------------------------------------------------------------------------------------
// Code
//-------------------------------------------------------------------------------------------------------------

template<typename T>
void NotifyPropertyChanged(T val) {
  Serial.print("Property changed to: "); Serial.println(val);
}

//T41Properties* T41Properties::instance = NULL;

T41Properties::T41Properties() {
  begin();
}

void T41Properties::begin() {
  // initialize static callback functions
  T41Update::SetUpdateFunctions(UpdateInfoBoxItem, SendCommand);
}

void T41Properties::SetPropertyDefaults() {
  int remoteStatus = CAT_CONTROL_T41 || CAT_CONTROL_REMOTE ? REMOTE_NOT_CONNECTED : REMOTE_NOT_AVAIL;

  RadioID.Set(RADIO_ID);
  RadioState.Set(RECONFIGURE_STATE);

  SampleRate.Set(192000.0);
  IntermediateFreq.Set(48000.0);

  // notify properties (not polled!)
  RemoteStatus.Init(remoteStatus, &ShowRemoteStatus);
  // make these a notify property
  MouseCenterTuneActive.Init(false, &HighlightTuneInc, T41_ITEM_MOUSE, false);
  NoiseFloor.Init(0, NULL, T41_ITEM_NOISE, false);
  FreqSpecScale.Init(1, &ShowSpectrumdBScale, T41_ITEM_SCALE, false); // 10 db/ scale
  CWFilterIndex.Init(5, 0, 5, true, &ShowOperatingStats, T41_ITEM_CW_FILTER, false);

  // polled properties
  RadioMode.Init(SSB_MODE, &UpdateModeDisplay, T41_ITEM_RADIO_MODE);
  DemodMode.Init(DEMOD_LSB, &UpdateModeDisplay, T41_ITEM_DEMOD_MODE);
  ActiveBand.Init(BAND_40M, 0, NUMBER_OF_BANDS - 1, true, &UpdateDisplayBand, T41_ITEM_BAND);
  TxPower.Init(DEFAULT_POWER_LEVEL, 1, 20, false, &RFPowerFollowup, T41_ITEM_POWER);

  CenterFreq.Init(CURRENT_FREQ_A, &UpdateDisplayFreq, T41_ITEM_FREQ);
  NCOFreq.Init(0, &CheckNCOFreqBounds, &UpdateDisplayNCOFreq, T41_ITEM_NCO);
  FilterHiCut.Init(3000, &UpdateDisplayFilters, T41_ITEM_FHI);
  FilterLoCut.Init(200, &UpdateDisplayFilters, T41_ITEM_FLO);

  // infobox properties
  AudioVolume.Init(30, MIN_AUDIO_VOLUME, MAX_AUDIO_VOLUME, false, T41_ITEM_VOL);
  AGCMode.Init(1, 0, 5 - 1, true, T41_ITEM_AGC);
  CenterTuneIndex.Init(DEFAULTFREQINDEX, 0, maxFreqIncIndex - 1, true, T41_ITEM_TUNE);
  FineTuneIndex.Init(DEFAULT_FT_INDEX, 0, maxFtIncIndex - 1, true, T41_ITEM_FINE);
  SpectrumZoom.Init(1, 0, MAX_ZOOM_ENTRIES - 1, true, T41_ITEM_ZOOM);
  LiveNoiseFloor.Init(0, 0, 2, true, T41_ITEM_FLOOR); // OFF=0, Auto=1, ON=2
  AutoNotch.Init(0, 0, 1, true, T41_ITEM_NOTCH);
  NoiseFilter.Init(0, 0, NR_OPTIONS, true, T41_ITEM_FILTER);
  Compressor.Init(0, 0, 1, true, T41_ITEM_COMPRESS);
  RFGain.Init(0, -60, 10, false, T41_ITEM_RFGAIN);
  RxEqualizer.Init(0, 0, 1, true, T41_ITEM_EQUALIZER);
  TxEqualizer.Init(0, 0, 1, true, T41_ITEM_EQUALIZER);
  CWDecoder.Init(DECODER_STATE, 0, 1, true, T41_ITEM_DECODER);
  KeyType.Init(STRAIGHT_KEY_OR_PADDLES, 0, 1, true, T41_ITEM_KEY);

  // *** TODO: these need notifications/updates added ***
  ActiveVFO.Init(VFO_A);
  PaddleDit.Init(KEYER_DIT_INPUT_TIP);
  PaddleDah.Init(KEYER_DAH_INPUT_RING);
  CurrentWPM.Init(DEFAULT_KEYER_WPM);
  SidetoneVolume.Init(20);
  CWTransmitDelay.Init(750);

  // properties w/o notifications or display updates
  InactiveFreq.Init(CURRENT_FREQ_B);
  InactiveBand.Init(BAND_40M);

  DroppedBlock.Init(0);
}

// helper functions
void T41Properties::Poll(bool updateDisplay) {
  bool updateRemote = RemoteStatus == REMOTE_CONNECTED;

  // *** TODO: consider order to minimize update duplication ***
  // *** TODO: consider refining updates as there is some duplication ***
  RadioMode.Poll(updateDisplay, updateRemote);
  DemodMode.Poll(updateDisplay, updateRemote);
  ActiveBand.Poll(updateDisplay, updateRemote);
  TxPower.Poll(updateDisplay, updateRemote);

  CenterFreq.Poll(updateDisplay, updateRemote);
  NCOFreq.Poll(updateDisplay, updateRemote);
  FilterHiCut.Poll(updateDisplay, updateRemote);
  FilterLoCut.Poll(updateDisplay, updateRemote);
}

void T41Properties::PollInfoBox(bool updateDisplay) {
  bool updateRemote = RemoteStatus == REMOTE_CONNECTED;

  AudioVolume.Poll(updateDisplay, updateRemote);
  AGCMode.Poll(updateDisplay, updateRemote);
  CenterTuneIndex.Poll(updateDisplay, updateRemote);
  FineTuneIndex.Poll(updateDisplay, updateRemote);
  SpectrumZoom.Poll(updateDisplay, updateRemote);
  LiveNoiseFloor.Poll(updateDisplay, updateRemote);
  NoiseFilter.Poll(updateDisplay, updateRemote);
  AutoNotch.Poll(updateDisplay, updateRemote);
  Compressor.Poll(updateDisplay, updateRemote);
  RFGain.Poll(updateDisplay, updateRemote);
  RxEqualizer.Poll(updateDisplay, updateRemote);
  TxEqualizer.Poll(updateDisplay, updateRemote);
  CWDecoder.Init(DECODER_STATE, 0, 1, true, T41_ITEM_DECODER);
  KeyType.Init(STRAIGHT_KEY_OR_PADDLES, 0, 1, true, T41_ITEM_KEY);
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
