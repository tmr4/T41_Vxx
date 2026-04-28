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

//int freqCorrectionFactor = 17578;   // AD3 source @ 5MHz
int freqCorrectionFactor = 0;   // gives

int equalizerRec[EQUALIZER_CELL_COUNT] = { 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100 };
int equalizerXmt[EQUALIZER_CELL_COUNT] = {0, 0, 100, 100, 100, 100, 100, 100, 100, 100, 100, 0, 0, 0};   // Provide equalizer optimized for SSB voice based on Neville's tests.  KF5N November 2, 2023

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

//-------------------------------------------------------------------------------------------------------------
// Code
//-------------------------------------------------------------------------------------------------------------
