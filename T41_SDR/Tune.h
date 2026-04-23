
//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

extern int CWFreqShift;
extern bool splitVFO;

//-------------------------------------------------------------------------------------------------------------
// Code
//-------------------------------------------------------------------------------------------------------------

void InitSI5351();

void SetSI5351FreqCorFactor(int factor);

void SetCenterTune(int tuneChange);

void SetNCOFreq(int newNCOFreq);
int CheckNCOFreqBounds(int f);

void ResetTuning();
void DoSplitVFO();

// *** hardware specific ***
void SetFreq(int freq, bool reset = false);
