
#include "property.h"

class T41Properties {
public:
  T41Properties();

  void begin();

  // T41 properties
  // *** template doesn't decrement with unsigned int ***
  Property<int> RemoteStatus;
  Property<int> CenterFreq;
  Property<int> NCOFreq;
  Property<int> AudioVolume;
  Property<int> FilterHiCut;
  Property<int> FilterLoCut;

  Property<int> ActiveVFO;
  Property<int> CurrentFreqA;
  Property<int> CurrentFreqB;

  // helper functions
  int TXRXFreq() { return CenterFreq + NCOFreq; }
  void SetFreq();

protected:
  void SetPropertyDefaults();

private:
  //static T41Properties* instance;
};

extern T41Properties t41;

// *** possible T41 properties ***
/*
from T41_Views (this is private data, first letter capitalized if property):

done:
  int centerFreq = 7048000;
  int audioVolume = 30;
  int ncoFreq = 0;
  int remoteStatus -1: not avail, 0: not connected (white), 1: connected (green), 2: connection lost (red)
  int fLoCut = -200; *** these were simple properties and didn't need an private value ***
  int fHiCut = -3000;
  int currentFreqA = 7048000;
  int currentFreqB = 7030000;

next:
  int activeVFO = 0; // VFO A
  int radioMode = 0;        // SSB_MODE;
  int currentDemodMode = 1; // DEMOD_LSB
  int currentBand = 1;      // BAND_40M;
  int transmitPowerLevel = 1;
  int tuneIndex = 6;
  int ftIndex = 3;

list:
  private bool centerTuneActive = false;
  //private int freqIncrement = 100000; *** maybe helper? ***
  //private int ftIncrement = 500;
  //private int currentBandA = 1; //BAND_40M; *** not property or data ***
  //private int currentBandB = 1; //BAND_40M;
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
  int radioMode;
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

  int activeVFO;
  int freqIncrement;

  int currentBand;
  int currentBandA;
  int currentBandB;
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
