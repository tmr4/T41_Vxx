// v12 specific hardware file

#include "..\SDT.h"

#include "..\Display.h"
#include "..\EEPROM.h"
#include "..\Utility.h"

#include "..\debug.h"

//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

// Configuration working global variables

//char versionSettings[10] = VERSION;
int AGCMode = 1;
int rfGainAllBands = 0;
int spectrumNoiseFloor = SPECTRUM_NOISE_FLOOR;
int tuneIndex = DEFAULTFREQINDEX;
int ftIndex = DEFAULT_FT_INDEX;
int transmitPowerLevel = DEFAULT_POWER_LEVEL;
int radioMode = SSB_MODE;  // 0 = SSB, 1 = CW, 2 = FT8
int nrOptionSelect = 0;
int currentScale = 1;  // 20 dB/division
int spectrumZoom = 1; // SPECTRUM_ZOOM_2
float spectrum_display_scale = 20.0;     // 30.0

int cwFilterIndex = 5;
int paddleDit = KEYER_DIT_INPUT_TIP;
int paddleDah = KEYER_DAH_INPUT_RING;
int decoderFlag = DECODER_STATE;  // Startup state for decoder
int keyType = STRAIGHT_KEY_OR_PADDLES;
int currentWPM =  DEFAULT_KEYER_WPM;
int sidetoneVolume = 20;
unsigned long cwTransmitDelay = 750; // CW exciter stays active for this amount of time after last CW atom

int activeVFO = 0;

int freqIncrement = 100000; // *** these need to be automated according to defines in config file ***
int ftIncrement = 500;

//int freqCorrectionFactor = 17578;   // AD3 source @ 5MHz
int freqCorrectionFactor = 0;   // gives

int equalizerRec[EQUALIZER_CELL_COUNT] = { 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100 };
int equalizerXmt[EQUALIZER_CELL_COUNT] = {0, 0, 100, 100, 100, 100, 100, 100, 100, 100, 100, 0, 0, 0};   // Provide equalizer optimized for SSB voice based on Neville's tests.  KF5N November 2, 2023

int currentMicThreshold = -10;
float currentMicCompRatio = 5.0;
float currentMicAttack = 0.1;
float currentMicRelease = 2.0;
int currentMicGain = -10;

int switchValues[NUMBER_OF_SWITCHES] = { 905, 853, 802, 752, 705, 653, 604, 556, 502, 451, 399, 344, 291, 237, 181, 124, 65, 4 };

float LPFcoeff = 0.0;
float NR_PSI = 0.0;
float NR_alpha = 0.95;
float NR_beta = 0.85;
float omegaN = 200.0;                       // PLL bandwidth 50.0 - 1000.0
float pll_fmax = +4000.0;

//float powerOutCW[NUMBER_OF_BANDS] = { 0.02, 0.02, 0.02, 0.02, 0.02, 0.02, 0.02 };
//float powerOutSSB[NUMBER_OF_BANDS] = { 0.03, 0.03, 0.03, 0.03, 0.03, 0.03, 0.03 };
//float32_t powerOutSSB[NUMBER_OF_BANDS] = { 1.0 };
float32_t powerOutSSB[NUMBER_OF_BANDS] = { 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0 };
float32_t powerOutCW[NUMBER_OF_BANDS] = { 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0 };

float CWPowerCalibrationFactor[NUMBER_OF_BANDS] = { 0.019, 0.019, .0190, .019, .019, .019, .019 };       // 0.019;
float SSBPowerCalibrationFactor[NUMBER_OF_BANDS] = { 0.008, 0.008, 0.008, 0.008, 0.008, 0.008, 0.008 };  // 0.008

//float IQAmpCorrectionFactor[NUMBER_OF_BANDS] = { 1, 0.976, 1, 1, 1, 1, 1 };
//float IQPhaseCorrectionFactor[NUMBER_OF_BANDS] = { 0, -0.011, 0, 0, 0, 0, 0 };
float IQAmpCorrectionFactor[NUMBER_OF_BANDS] = { 1, 1, 1, 1, 1, 1, 1 };
float IQPhaseCorrectionFactor[NUMBER_OF_BANDS] = { 0, 0, 0, 0, 0, 0, 0 };

float IQXAmpCorrectionFactor[NUMBER_OF_BANDS] = { 1, 1.004, 1, 1, 1, 1, 1 };
float IQXPhaseCorrectionFactor[NUMBER_OF_BANDS] = { 0, -0.01, 0, 0, 0, 0, 0 };

long favoriteFreqs[13] = { 3560000, 3690000, 7030000, 7200000, 14060000, 14200000, 21060000, 21285000, 28060000, 28365000, 5000000, 10000000, 15000000 };
//int lastFrequencies[NUMBER_OF_BANDS][2] = { { 3548000, 3560000 }, { 7048000, 7030000 }, { 14048000, 14100000 }, { 18116000, 18110000 }, { 21048000, 21150000 }, { 24937000, 24930000 }, { 28048000, 28200000 } };
int lastFrequencies[NUMBER_OF_BANDS][2] = { { 3548000, 3560000 }, { 7074000, 7030000 }, { 14074000, 14100000 }, { 18116000, 18110000 }, { 21048000, 21150000 }, { 24937000, 24930000 }, { 28048000, 28200000 } };

char mapFileName[50];
char myCall[10];
char myTimeZone[10];
int separationCharacter = (int) '.';

int paddleFlip = PADDLE_FLIP;
int sdCardPresent = 0;  // Do they have an micro SD card installed?

float myLat = MY_LAT;
float myLong = MY_LON;
//int currentNoiseFloor[NUMBER_OF_BANDS] = { 0, 50, 0, 0, 0, 0, 0 };
int currentNoiseFloor[NUMBER_OF_BANDS] = { 0, 0, 0, 0, 0, 0, 0 };
int compressorFlag = 0;

int buttonThresholdPressed = 944;   // switchValues[0] + WIGGLE_ROOM
int buttonThresholdReleased = 964;  // buttonThresholdPressed + WIGGLE_ROOM
int buttonRepeatDelay = 300000;     // Increased to 300000 from 200000 to better handle cheap, wornout buttons.

//-------------------------------------------------------------------------------------------------------------
// Code
//-------------------------------------------------------------------------------------------------------------
