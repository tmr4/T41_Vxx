
#include "property.h"

class T41Properties {
public:
  T41Properties();

  void begin();
  void SetPropertyDefaults();

  // T41 properties (w/ old T41 working variable)
  // *** interestingly CenterFreq is not updated when the center tune encoder is moved; examine need for this property ***
  Property<int> CenterFreq; // centerFreq
  Property<int> NCOFreq; // NCOFreq
  // *** template doesn't decrement this with unsigned int ***
  Property<int> AudioVolume; // audioVolume

protected:

private:

};

extern T41Properties t41;

// *** possible T41 properties ***
/*
from T41_Views (this is private data, first letter capitalized if property):

done:
  private long centerFreq = 7048000;
  private int audioVolume = 30;
  private int ncoFreq = 0;

list:
  private int activeVFO = 0; // VFO A
  private bool centerTuneActive = false;
  private int freqIndex = 6;
  private int ftIndex = 3;
  private int freqIncrement = 100000;
  private int ftIncrement = 500;
  private long currentFreqA = 7048000;
  private long currentFreqB = 7030000;
  private int currentDemod = 1; // DEMOD_LSB
  private int xmtMode = 0; // SSB_MODE;
  private int currentBand = 1; // BAND_40M;
  //private int currentBandA = 1; //BAND_40M; *** not property or data ***
  //private int currentBandB = 1; //BAND_40M;
  private int currentNF = 0;
  private int agcMode = 1;
  private int liveNoiseFloorFlag = 0;
  private int transmitPowerLevel = 1;
  //private int fLoCut = -200; *** these were simple properties and didn't need an private value ***
  //private int fHiCut = -3000;
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
  int currentFreqA;
  int currentFreqB;
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
