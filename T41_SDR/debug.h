
#include "src\hardwareConfig.h"

//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

//#define DEBUG_LOOP
//#define ALT_ISR

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

void EnterLoop();
void ExitLoop();
void ButtonInfoOut(int valPin, int pushButtonSwitchIndex);

void memInfo();
void getFreeITCM();
