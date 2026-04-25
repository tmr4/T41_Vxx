
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
  Property<int> RemoteStatus;  // notify on change, not polled

  // polled properties
  Property<int> RadioMode;
  Property<int> DemodMode;

  Property<int> CenterFreq;
  Property<int> NCOFreq;
  Property<int> AudioVolume;
  Property<int> FilterHiCut;
  Property<int> FilterLoCut;
  Property<int> ActiveBand;

  // properties w/o notifications or display updates
  Property<int> ActiveVFO;
  Property<int> InactiveFreq;
  Property<int> InactiveBand;

  // helper functions
  void Poll(bool updateDisplay, bool updateRemote);
  void PollInfoBox(bool updateDisplay, bool updateRemote);

  int ActiveFreq() { return CenterFreq + NCOFreq; }
  int GetFreqA() { return ActiveVFO == VFO_A ? ActiveFreq() : InactiveFreq; }
  int GetFreqB() { return ActiveVFO == VFO_B ? ActiveFreq() : InactiveFreq; }
  void SetFreqA(int f);
  void SetFreqB(int f);
  void SwapActiveVFO();

protected:
  void SetPropertyDefaults();

private:
  //static T41Properties* instance;
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
  int tuneIndex = 6;
  int ftIndex = 3;

list:
  private bool centerTuneActive = false;
  //private int freqIncrement = 100000; *** maybe helper? ***
  //private int ftIncrement = 500;
  private int currentNF = 0;
  private int agcMode = 1;
  private int liveNoiseFloorFlag = 0;
  private bool dataFlag = false;

*/
/*
// old EEPROMData
typedef struct {
  char versionSettings[10];
  int AGCMode;
  int rfGainAllBands;
  int spectrumNoiseFloor;
  int tuneIndex;
  int ftIndex;
  float32_t transmitPowerLevel;
  int t41.RadioMode;
  int nrOptionSelect;
  int currentScale;
  long spectrumZoom;
  float spectrum_display_scale;

  int cwFilterIndex;
  int paddleDit;
  int paddleDah;
  int decoderFlag;
  int keyType;
  int currentWPM;
  int sidetoneVolume;
  unsigned long cwTransmitDelay;

  int freqIncrement;

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
  int currentNoiseFloor[NUMBER_OF_BANDS];
  int compressorFlag;

  int buttonThresholdPressed;
  int buttonThresholdReleased;
  int buttonRepeatDelay;
} config_t;
*/
//extern config_t EEPROMData;
