#pragma once

//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

extern int calibrateItem;

extern int freqCorrectionFactor;

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

//-------------------------------------------------------------------------------------------------------------
// Code
//-------------------------------------------------------------------------------------------------------------

void CalibrationInit(int calType);
void CalibrationExit();
void CalibrationLoop();
void CalibrationReset();
