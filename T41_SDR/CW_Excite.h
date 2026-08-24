#pragma once

//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

extern uint8_t keyPressedOn;

extern bool pwrScale;

//-------------------------------------------------------------------------------------------------------------
// Code
//-------------------------------------------------------------------------------------------------------------

void KeyTipOn();
void KeyRingOn();
void CW_ExciterIQData(int state = ON, bool ramp = false, bool pause = true, float timeAdjust = 0.0);
void CreateCWSignal(unsigned long signalLength);

void CWTransmit(int pin);
void CWTransmitPaddle();
