// v11 specific hardware file

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
int currentScale = 1;  // 20 dB/division

int cwFilterIndex = 5;
int paddleDit = KEYER_DIT_INPUT_TIP;
int paddleDah = KEYER_DAH_INPUT_RING;
int decoderFlag = DECODER_STATE;  // Startup state for decoder
int keyType = STRAIGHT_KEY_OR_PADDLES;
int currentWPM =  DEFAULT_KEYER_WPM;
int sidetoneVolume = 20;
unsigned long cwTransmitDelay = 750; // CW exciter stays active for this amount of time after last CW atom

int freqCorrectionFactor = 1200;

int equalizerRec[EQUALIZER_CELL_COUNT] = { 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100 };
int equalizerXmt[EQUALIZER_CELL_COUNT] = {0, 0, 100, 100, 100, 100, 100, 100, 100, 100, 100, 0, 0, 0};   // Provide equalizer optimized for SSB voice based on Neville's tests.  KF5N November 2, 2023

//float powerOutCW[NUMBER_OF_BANDS] = { 0.02, 0.02, 0.02, 0.02, 0.02, 0.02, 0.02 };
//float powerOutSSB[NUMBER_OF_BANDS] = { 0.03, 0.03, 0.03, 0.03, 0.03, 0.03, 0.03 };
//float32_t powerOutSSB[NUMBER_OF_BANDS] = { 1.0 };
float32_t powerOutSSB[NUMBER_OF_BANDS] = { 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0 };
float32_t powerOutCW[NUMBER_OF_BANDS] = { 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0 };

//float CWPowerCalibrationFactor[NUMBER_OF_BANDS] = { 0.019, 0.06, .0190, .019, .019, .019, .019 };       // 0.019;
//float SSBPowerCalibrationFactor[NUMBER_OF_BANDS] = { 0.008, 0.008, 0.008, 0.008, 0.008, 0.008, 0.008 };  // 0.008
//float CWPowerCalibrationFactor[NUMBER_OF_BANDS] =  { 1.0, 1.5858, 1.0, 1.0, 1.0, 1.0, 1.0 };
//float CWPowerCalibrationFactor[NUMBER_OF_BANDS] =  { 1.0, 3.0, 1.0, 1.0, 1.0, 1.0, 1.0 };
//                                                   80m     40m     20m     17m  15m     12m      10m
//float CWPowerCalibrationFactor[NUMBER_OF_BANDS] =  { 4.1732, 4.8860, 6.8056, 1.0, 9.9390, 12.2837, 1.0 };
//float CWPowerCalibrationFactor[NUMBER_OF_BANDS] =  { 3.6289, 4.2487, 5.2351, 1.0, 7.6453, 9.4490, 1.0 };
//float CWPowerCalibrationFactor[NUMBER_OF_BANDS] =  { 0.7932, 0.9947, 1.6138, 1.0, 2.7056, 3.1022, 1.0 };
float CWPowerCalibrationFactor[NUMBER_OF_BANDS] = { 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0 };
float SSBPowerCalibrationFactor[NUMBER_OF_BANDS] = { 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0 };
float FT8PowerCalibrationFactor[NUMBER_OF_BANDS] = { 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0 };

float CWPowerEqnCalFactor[NUMBER_OF_BANDS] = { 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1 };

//float IQAmpCorrectionFactor[NUMBER_OF_BANDS] = { 1, 1, 1, 1, 1, 1, 1 };
//float IQPhaseCorrectionFactor[NUMBER_OF_BANDS] = { 0, 0, 0, 0, 0, 0, 0 };

// all band receive auto calibration performed on 8/18/2025 (*** 10m band skipped ***)
//                                                  80m     40m    20m    17m    15m    12m     10m
float IQAmpCorrectionFactor[NUMBER_OF_BANDS] =   {  0.988,  0.988, 1.009, 1.030, 1.065, 1.092,  1.000 };
float IQPhaseCorrectionFactor[NUMBER_OF_BANDS] = { -0.010, -0.027, 0.059, 0.035, 0.080, 0.122,  0.000 };

//float IQXAmpCorrectionFactor[NUMBER_OF_BANDS] = { 1, 1, 1, 1, 1, 1, 1 };
//float IQXPhaseCorrectionFactor[NUMBER_OF_BANDS] = { 0, 0, 0, 0, 0, 0, 0 };

// all band transmit auto calibration performed on 8/18/2025 (*** 10m band skipped ***)
//                                                   80m     40m     20m     17m     15m     12m    10m
float IQXAmpCorrectionFactor[NUMBER_OF_BANDS] =   {  1.010,  1.003,  0.965,  0.930,  0.900,  0.880, 1.000 };
float IQXPhaseCorrectionFactor[NUMBER_OF_BANDS] = { -0.010, -0.010, -0.027, -0.055, -0.100, -0.090, 0.000 };
//float IQXAmpCorrectionFactor[NUMBER_OF_BANDS] =   {  1.010,  1.010,  0.965,  0.930,  0.900,  0.880, 1.000 };
//float IQXPhaseCorrectionFactor[NUMBER_OF_BANDS] = { -0.010, -0.006, -0.027, -0.055, -0.100, -0.090, 0.000 };

//-------------------------------------------------------------------------------------------------------------
// Code
//-------------------------------------------------------------------------------------------------------------
