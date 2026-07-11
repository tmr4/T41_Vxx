
#include "SDT.h"

#include "Display.h"
#include "Encoders.h"
#include "Filter.h"
#include "Menu.h"
#include "MenuProc.h"
#include "Tune.h"

//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

bool splitVFO;

int CWFreqShift = 750;
//int CWFreqShift = 0;

//-------------------------------------------------------------------------------------------------------------
// Code
//-------------------------------------------------------------------------------------------------------------

/*****
  Purpose: Reset tuning to center
           NCOFreq is set to zero

  Parameter list:
  void

  Return value:
  void
*****/
void ResetTuning() {
  t41.CenterFreq += t41.NCOFreq;
  t41.NCOFreq = 0;

  SetFreq(t41.CenterFreq);

  //UpdateDisplayNCOFreq();
}

/*****
  Purpose: Adjust center tuning frequency
           NCOFreq is unchanged

  Parameter list:
    long tuneChange - amound to change center freq
*****/
void SetCenterTune(int tuneChange) {
  t41.CenterFreq += tuneChange;

  SetFreq(t41.ActiveFreq());
}

int CheckNCOFreqBounds(int f) {
  int freq = f;
  int lowSideAdj = 0, highSideAdj = 0;

  switch(t41.DemodMode) {
    case DEMOD_USB:
    case DEMOD_PSK31_WAV:
    case DEMOD_PSK31:
    case DEMOD_FT8:
    case DEMOD_FT8_WAV:
      lowSideAdj = 0;
      highSideAdj = t41.FilterHiCut;
      break;

    case DEMOD_LSB:
      lowSideAdj = t41.FilterHiCut;
      highSideAdj = 0;
      break;

    case DEMOD_AM:
    case DEMOD_SAM:
      break;

    case DEMOD_NFM:
      break;
  }

  // recenter at band edges
  if(t41.SpectrumZoom != 0) {
    if((f + highSideAdj) >= (t41.SampleRate / 2.0 / (1 << t41.SpectrumZoom))) {
      freq += highSideAdj;
      resetTuningFlag = true;
    } else if((f - lowSideAdj) <= (-t41.SampleRate / 2.0 / (1 << t41.SpectrumZoom))) {
      freq -= lowSideAdj;
      resetTuningFlag = true;
    }
  } else if(f > 142000 || f < -43000) {  // Offset tuning window in zoom 1x
    resetTuningFlag = true;
  }

  return freq;
}

/*****
  Purpose: sets frequancy for selected band and update filters accordingly
*****/
FLASHMEM void SetupBandFreq(int freq) {
  SetFreq(freq);

  SetupDemodFilterBW();
}

// *** TODO: display dependent ***
FLASHMEM void SplitVFOFollowup() {
  // *** TODO: need to reestablish "Split Active" that didn't work in ver49.2k ***
  //tft.setTextColor(RA8875_RED);
  //tft.setCursor(FILTER_PARAMETERS_X + 180, FILTER_PARAMETERS_Y + 6);
  //tft.print("Split Active");
  //splitVFO = true;
}

/*****
  Purpose: Set VFO A to receive frequency and VFO B to the transmit frequency
*****/
FLASHMEM void DoSplitVFO() {
  //t41.CurrentFreqB = t41.CurrentFreqA;

  // GetMenuValue(minValue, maxValue, startValue, increment, prompt, valueOffset)
  //GetMenuValue(-40, 30, &t41.CurrentFreqB, SPLIT_INCREMENT, "Xmit offset:", 200, NULL, NULL, &SplitVFOFollowup);
  //GetMenuValue(-40, 30, (int*)&t41.CurrentFreqB, 500, "Xmit offset:", 200, NULL, NULL, &SplitVFOFollowup);
}
