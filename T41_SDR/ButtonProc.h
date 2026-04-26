
//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

extern bool lowerAudioFilterActive;

extern bool nfmBWFilterActive;

//-------------------------------------------------------------------------------------------------------------
// Code
//-------------------------------------------------------------------------------------------------------------

void UpdateBand(int from);
void ChangeBand(int change);
void ChangeBand(long newFreq);
void ButtonFilter();
void ButtonMode();
void ButtonDemodMode();
void ChangeDemodMode(int mode, bool notify = true);
void ChangeMode(int mode, int demod = -1, bool notify = true);
void ButtonNR();
void ButtonNotchFilter();
void ButtonFrequencyEntry();
void ToggleCWDecoder();

void ChangeFreqIncrement(int change, bool notify = true);
void ChangeFtIncrement(int change, bool notify = true);
