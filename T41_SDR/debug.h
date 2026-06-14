#pragma once

#include "src\hardwareConfig.h"

//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

#ifdef PROFILER_ACTIVE
#define SETPROFILEPIN(pin) digitalWriteFast(pin, HIGH)
#define RESETPROFILEPIN(pin) digitalWriteFast(pin, LOW)
#define TOGGLEPROFILEPIN(pin) digitalToggleFast(pin)
//#define TOGGLEPROFILEPIN(pin) digitalWrite(pin, !digitalRead(pin))
#else
#define SETPROFILEPIN(pin)
#define RESETPROFILEPIN(pin)
#define TOGGLEPROFILEPIN(pin)
#endif

extern int loopCounter;
extern bool memCheck;

//-------------------------------------------------------------------------------------------------------------
// Code
//-------------------------------------------------------------------------------------------------------------

void memInfo();
void getFreeITCM();
