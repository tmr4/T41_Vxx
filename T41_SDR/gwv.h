
//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

// Configuration working global variables
// commented lines have no corresponding global extern

//extern char versionSettings[];
extern int AGCMode;
extern int rfGainAllBands;
extern int spectrumNoiseFloor;
extern int transmitPowerLevel;
extern int nrOptionSelect;
extern int currentScale;
extern int spectrumZoom;
extern float spectrum_display_scale;

extern int cwFilterIndex;
extern int paddleDit;
extern int paddleDah;
extern int decoderFlag;
extern int keyType;
extern int currentWPM;
extern int sidetoneVolume;
extern unsigned long cwTransmitDelay;

extern int freqCorrectionFactor;

extern int equalizerRec[];
extern int equalizerXmt[];

extern int currentMicThreshold;
extern float currentMicCompRatio;
extern float currentMicAttack;
extern float currentMicRelease;
extern int currentMicGain;

extern int switchValues[];

extern float LPFcoeff;
extern float NR_PSI;
extern float NR_alpha;
extern float NR_beta;
extern float omegaN ;
extern float pll_fmax;

extern float powerOutCW[];
extern float powerOutSSB[];
extern float CWPowerCalibrationFactor[];
extern float SSBPowerCalibrationFactor[];
extern float FT8PowerCalibrationFactor[];
extern float IQAmpCorrectionFactor[];
extern float IQPhaseCorrectionFactor[];
extern float IQXAmpCorrectionFactor[];
extern float IQXPhaseCorrectionFactor[];

extern float CWPowerEqnCalFactor[];

extern long favoriteFreqs[13];
extern int lastFrequencies[][2];

extern char mapFileName[];
extern char myCall[];
extern char myTimeZone[];
extern int  separationCharacter;

extern int paddleFlip;
extern int sdCardPresent;

extern float myLat;
extern float myLong;
extern int currentNoiseFloor[];
extern int compressorFlag;

extern int buttonThresholdPressed;
extern int buttonThresholdReleased;
extern int buttonRepeatDelay;

//-------------------------------------------------------------------------------------------------------------
// Code
//-------------------------------------------------------------------------------------------------------------
