
#include "property.h"
#pragma once

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
  ReadOnlyProperty<int> RadioState;

  ReadOnlyProperty<float> SampleRate;
  ReadOnlyProperty<float> IntermediateFreq;

  // *** Property template doesn't decrement with unsigned int ***
  // notify on change, not polled
  Property<int> RemoteStatus;
  Property<int> MouseCenterTuneActive;
  Property<int> NoiseFloor;
  Property<int> FreqSpecScale;

  // CW related
  Property<int> CWFilterIndex;

  // polled properties
  Property<int> RadioMode;
  Property<int> DemodMode;
  Property<int> ActiveBand;
  Property<int> TxPower;

  Property<int> CenterFreq;
  Property<int> NCOFreq;
  Property<int> FilterHiCut;
  Property<int> FilterLoCut;

  // infobox properties
  Property<int> AudioVolume;
  Property<int> AGCMode;
  Property<int> CenterTuneIndex;
  Property<int> FineTuneIndex;
  Property<int> SpectrumZoom;
  Property<int> LiveNoiseFloor;
  Property<int> NoiseFilter;
  Property<int> AutoNotch;
  Property<int> Compressor;
  Property<int> RFGain;
  Property<int> RxEqualizer;
  Property<int> TxEqualizer;
  Property<int> CWDecoder;
  Property<int> KeyType;

  // properties currently w/o notifications or display updates
  Property<int> ActiveVFO;
  Property<int> PaddleDit;
  Property<int> PaddleDah;
  Property<int> CurrentWPM;
  Property<int> SidetoneVolume;
  Property<unsigned long> CWTransmitDelay; // CW exciter stays active for this amount of time after last CW atom

  Property<int> InactiveFreq;
  Property<int> InactiveBand;

  Property<int> DroppedBlock; // blocks were dropped since last update: 0=false, 1=true

  // helper functions
  void SetPropertyDefaults();

  void Poll(bool updateDisplay);
  void PollInfoBox(bool updateDisplay);

  int ActiveFreq() { return CenterFreq + NCOFreq; }
  int GetFreqA() { return ActiveVFO == VFO_A ? ActiveFreq() : InactiveFreq; }
  int GetFreqB() { return ActiveVFO == VFO_B ? ActiveFreq() : InactiveFreq; }
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
