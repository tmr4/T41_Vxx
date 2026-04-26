
#include "property.h"

//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

class T41Properties {
public:
  T41Properties();

  void begin();

  // T41 properties
  // *** template doesn't decrement with unsigned int ***

  // notify on change, not polled
  Property<int> RemoteStatus;
  Property<int> MouseCenterTuneActive;
  Property<int> NoiseFloor;

  // polled properties
  Property<int> RadioMode;
  Property<int> DemodMode;

  Property<int> CenterFreq;
  Property<int> NCOFreq;
  Property<int> FilterHiCut;
  Property<int> FilterLoCut;
  Property<int> ActiveBand;

  // infobox properties
  Property<int> AudioVolume;
  Property<int> AGCMode;
  Property<int> CenterTuneIndex;
  Property<int> FineTuneIndex;
  Property<int> SpectrumZoom;
  Property<int> LiveNoiseFloor;

  // properties w/o notifications or display updates
  Property<int> ActiveVFO;
  Property<int> InactiveFreq;
  Property<int> InactiveBand;

  // helper functions
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

protected:
  void SetPropertyDefaults();

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


// *** possible T41 properties ***
/*
from T41_Views (this is private data, first letter capitalized if property):

next:
  int transmitPowerLevel = 1;
  int spectrumNoiseFloor;
  int rfGainAllBands;
  private int currentNF = 0;

*/
/*
// old EEPROMData
typedef struct {
  int nrOptionSelect;
  int currentScale;
  float spectrum_display_scale;

  int cwFilterIndex;
  int paddleDit;
  int paddleDah;
  int decoderFlag;
  int keyType;
  int currentWPM;
  int sidetoneVolume;
  unsigned long cwTransmitDelay;

  int freqCorrectionFactor;

  int equalizerRec[EQUALIZER_CELL_COUNT];
  int equalizerXmt[EQUALIZER_CELL_COUNT];

  int currentMicThreshold;
  float currentMicCompRatio;
  float currentMicAttack;
  float currentMicRelease;
  int currentMicGain;

  int switchValues[NUMBER_OF_SWITCHES];

  float LPFcoeff;
  float NR_PSI;
  float NR_alpha;
  float NR_beta;
  float omegaN;
  float pll_fmax;

  float powerOutCW[NUMBER_OF_BANDS];
  float powerOutSSB[NUMBER_OF_BANDS];
  float CWPowerCalibrationFactor[NUMBER_OF_BANDS];
  float SSBPowerCalibrationFactor[NUMBER_OF_BANDS];
  float IQAmpCorrectionFactor[NUMBER_OF_BANDS];
  float IQPhaseCorrectionFactor[NUMBER_OF_BANDS];
  float IQXAmpCorrectionFactor[NUMBER_OF_BANDS];
  float IQXPhaseCorrectionFactor[NUMBER_OF_BANDS];

  long favoriteFreqs[13];
  int lastFrequencies[NUMBER_OF_BANDS][2];

  char mapFileName[50];
  char myCall[10];
  char myTimeZone[10];
  int  separationCharacter;

  int paddleFlip;
  int sdCardPresent;

  float myLong;
  float myLat;
  int compressorFlag;

  int buttonThresholdPressed;
  int buttonThresholdReleased;
  int buttonRepeatDelay;
} config_t;
*/
//extern config_t EEPROMData;
