// vPS version of this is empty for now but still required for overall project

#pragma once

#ifdef PROJECTSYSTEM_ENCODER_MCP

#include <stdint.h>
#include "Rotary_V12.h"

//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

//#define FRONT_PANEL_POLLING_OPS

extern int volumeFunction;

// Define a structure to hold the results of built-in-test routine
typedef struct {
  bool RF_I2C_present;
  bool RF_Si5351_present;
  bool BPF_I2C_present;
  bool V12_LPF_I2C_present;
  bool V12_LPF_AD7991_present;
  bool FRONT_PANEL_I2C_1_present;
  bool FRONT_PANEL_I2C_2_present;
  byte AD7991_I2C_ADDR;
} I2C;

extern I2C bit_results;

//-------------------------------------------------------------------------------------------------------------
// Code
//-------------------------------------------------------------------------------------------------------------

void InitFrontPanel();

void PollFrontPanel();

#endif
