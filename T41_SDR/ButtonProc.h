#pragma once

//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

extern bool lowerAudioFilterActive;

extern bool nfmBWFilterActive;

//-------------------------------------------------------------------------------------------------------------
// Code
//-------------------------------------------------------------------------------------------------------------

void ChangeBand(int change, bool notify = true);
//void ChangeBand(long newFreq);
void ButtonFilter();
void ButtonMode();
void ButtonDemodMode();
void ChangeDemodMode(int mode, bool notify = true);
void ChangeMode(int mode, int demod = -1, bool notify = true);
void ButtonNotchFilter();
void ButtonFrequencyEntry();
void ToggleCWDecoder();

void ChangeFreqIncrement(int change, bool notify = true);
void ChangeFtIncrement(int change, bool notify = true);
