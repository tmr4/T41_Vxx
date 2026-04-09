// minimum hardware source file

#include <Arduino.h>

#include "..\Encoders.h"

//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

// *** TODO: looks like this can go to hardware specific files ***
//extern float32_t HP_DC_Filter_Coeffs2[];

//-------------------------------------------------------------------------------------------------------------
// Code
//-------------------------------------------------------------------------------------------------------------

// General
float CalcSignalStrength() { return 0.0; }

// Button.cpp
int ReadSelectedPushButton() { return -1; }

// Encoders.cpp
int ReadTuneEncoder() { return 0; }

// Menu.cpp
int ProcessButtonPress(int valPin) { return 0; }

// MenuProc.cpp
FLASHMEM void RFOptions() {}
FLASHMEM void CalibrateOptions() {}

// Process.cpp
void RemoveDCBias() {}

// T41_SDR.ino
void InitHardware() {}

//void SoftResetHardware() {}
void SoftResetHardware() {
  // *** TODO: this is required for min startup, find out why and possibly change ***
  posFilterEncoder = 0;
  lastFilterEncoder = 1; // force initial update
}

void ConfigRadioStateHardware() {}
void HardwareLoopStart() { delay(10); } // *** TODO: some failures reprogramming Teensy with min hardware, check if this fixes issue ***
