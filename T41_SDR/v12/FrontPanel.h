// v12 specific hardware file

#pragma once

#include <stdint.h>
#include "Rotary_V12.h"

//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

//#define AUDIO_VOLUME 0
//#define MIC_GAIN 1
//#define AGC_GAIN 2
//#define SIDETONE_VOLUME 3
//#define NOISE_FLOOR_LEVEL 4
//#define SQUELCH_LEVEL 5

#define FRONT_PANEL_POLLING_OPS

extern int ButtonPressed;
extern int volumeFunction;
//extern int my_ptt;

//-------------------------------------------------------------------------------------------------------------
// Code
//-------------------------------------------------------------------------------------------------------------
void InitFrontPanel();
//void FrontPanelSetLed(int led, uint8_t state);
//void PTT_Interrupt();

void Mcp1Isr();
void Mcp2Isr();
