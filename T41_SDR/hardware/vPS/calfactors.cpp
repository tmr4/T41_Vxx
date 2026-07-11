// generic hardware calibration file

#include "..\SDT.h"

//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

int freqCorrectionFactor = 0;

float32_t powerOutSSB[NUMBER_OF_BANDS] = { 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0 };
float32_t powerOutCW[NUMBER_OF_BANDS] = { 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0 };

float CWPowerCalibrationFactor[NUMBER_OF_BANDS] = { 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0 };
float SSBPowerCalibrationFactor[NUMBER_OF_BANDS] = { 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0 };

float IQAmpCorrectionFactor[NUMBER_OF_BANDS] = { 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0 };
float IQPhaseCorrectionFactor[NUMBER_OF_BANDS] = { 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0 };

float IQXAmpCorrectionFactor[NUMBER_OF_BANDS] = { 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0 };
float IQXPhaseCorrectionFactor[NUMBER_OF_BANDS] = { 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0 };

//-------------------------------------------------------------------------------------------------------------
// Code
//-------------------------------------------------------------------------------------------------------------
