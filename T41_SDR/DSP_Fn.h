#pragma once

//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

//-------------------------------------------------------------------------------------------------------------
// Code
//-------------------------------------------------------------------------------------------------------------

void NoiseBlanker(float32_t* inputsamples, float32_t* outputsamples);
void AGCLoadValues();
void AGCPrep();
void AGC();

void ApplyIQCorrectionFactors(float *bufI, float *bufQ, float amp, float phase, uint32_t bufSize);
void SelectSideBand(float32_t *buf, uint32_t bufSize);
