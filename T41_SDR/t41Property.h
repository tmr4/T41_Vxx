#pragma once

#include "property.h"

//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

#define EQUALIZER_CELL_COUNT     14

class T41Properties {
public:
  T41Properties();

  void begin();

  // T41 properties
  /*
    RadioState is a read-only property.  It's intended to be set only in the main loop
    or in the execution loops of routines that bypass that loop, such as during
    calibration. As a read-only property, RadioState can't be assigned a value directly.
    The Set method must be used to assign a value.
  */
  ReadOnlyProperty<int> RadioID{RADIO_ID, "ID"_cat};
  ReadOnlyProperty<int> RadioRole{RADIO_ROLE};
  Property<int> RadioState{RECONFIGURE_STATE};

  Property<float> SampleRate{192000.0};
  Property<float> IntermediateFreq{48000.0};

  // *** Property template doesn't decrement with unsigned int ***
  // notify on change, not polled
  Property<int> RemoteStatus{REMOTE_NOT_AVAIL};
  Property<int> MouseCenterTuneActive{false, "FS"_cat};
  Property<int> NoiseFloor{0, "NF"_cat};
  Property<int> FreqSpecScale{1};

  // CW related
  Property<int> CWFilterIndex{5};

  // polled properties
  Property<int> RadioMode{SSB_MODE, "ME"_cat};
  Property<int> DemodMode{DEMOD_LSB, "MD"_cat};
  Property<int> ActiveBand{BAND_40M, "BD"_cat};
  Property<int> TxPower{DEFAULT_POWER_LEVEL, "PC"_cat};

  Property<int> CenterFreq{CURRENT_FREQ_A, "FC"_cat};
  Property<int> NCOFreq{0, "FF"_cat};
  Property<int> FilterHiCut{3000, "NH"_cat};
  Property<int> FilterLoCut{200, "NL"_cat};

  // infobox properties
  Property<int> AudioVolume{30, "VO"_cat};
  Property<int> AGCMode{1, "GT"_cat};
  Property<int> CenterTuneIndex{DEFAULTFREQINDEX, "F0"_cat};
  Property<int> FineTuneIndex{DEFAULT_FT_INDEX, "F1"_cat};
  Property<int> SpectrumZoom{1, "ZM"_cat};
  Property<int> LiveNoiseFloor{0, "NG"_cat};
  Property<int> NoiseFilter{0, "N1"_cat};
  Property<int> AutoNotch{0};
  Property<int> Compressor{0};
  Property<int> RFGain{0, "PG"_cat};
  Property<int> RxEqualizer{0};
  Property<int> TxEqualizer{0};
  Property<int> CWDecoder{DECODER_STATE};
  Property<int> KeyType{STRAIGHT_KEY_OR_PADDLES};

  // properties currently w/o notifications or display updates
  Property<int> ActiveVFO{VFO_A};
  Property<int> PaddleDit{KEYER_DIT_INPUT_TIP};
  Property<int> PaddleDah{KEYER_DAH_INPUT_RING};
  Property<int> CurrentWPM{DEFAULT_KEYER_WPM};
  Property<int> SidetoneVolume{20};
  Property<unsigned long> CWTransmitDelay{750}; // CW exciter stays active for this amount of time after last CW atom

  Property<int> InactiveFreq{CURRENT_FREQ_B};
  Property<int> InactiveBand{BAND_40M};

  Property<int> DroppedBlock{0}; // blocks were dropped since last update: 0=false, 1=true

  // helper functions
  void SetPropertyDefaults();

  void Poll(bool updateDisplay);
  void PollInfoBox(bool updateDisplay);

  int ActiveFreq() { return CenterFreq + NCOFreq; }
  int GetFreqA() { return ActiveVFO == VFO_A ? ActiveFreq() : InactiveFreq.value; } // *** InactiveFreq.value needed to avoid compiler error with template base class ***
  int GetFreqB() { return ActiveVFO == VFO_B ? ActiveFreq() : InactiveFreq.value; }
  int FreqIncrement() { return freqIncValues[CenterTuneIndex]; }
  int FtIncrement() { return ftIncValues[FineTuneIndex]; }

  void SetFreqA(int f);
  void SetFreqB(int f);
  void SwapActiveVFO();

  int equalizerRx[EQUALIZER_CELL_COUNT] = { 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100 };
  int equalizerTx[EQUALIZER_CELL_COUNT] = {0, 0, 100, 100, 100, 100, 100, 100, 100, 100, 100, 0, 0, 0};   // Provide equalizer optimized for SSB voice based on Neville's tests.  KF5N November 2, 2023

  protected:

private:
  static constexpr int maxFreqIncIndex = 8;
  static constexpr int freqIncValues[maxFreqIncIndex] = { 10, 50, 100, 250, 1000, 10000, 100000, 1000000 };

  static constexpr int maxFtIncIndex = 4;
  static constexpr int ftIncValues[maxFtIncIndex] = { 10, 50, 250, 500 };

};

extern T41Properties t41;

//-------------------------------------------------------------------------------------------------------------
// Forwards
//-------------------------------------------------------------------------------------------------------------

//-------------------------------------------------------------------------------------------------------------
// Code
//-------------------------------------------------------------------------------------------------------------
