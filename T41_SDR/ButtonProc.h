
//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

#define MAX_ZOOM_ENTRIES      5

extern bool lowerAudioFilterActive;
extern int liveNoiseFloorFlag;

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
void ChangeDemodMode(int mode);
void ChangeMode(int mode, int demod = -1, bool notify = true);
void ButtonNR();
void ButtonNotchFilter();
void ButtonFrequencyEntry();
void ToggleLiveNoiseFloorFlag();
void ToggleCWDecoder();

void ChangeFreqIncrement(int change);
void ChangeFtIncrement(int change);
