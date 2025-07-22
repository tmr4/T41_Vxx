
#include "..\SDT.h"

#include "..\Button.h"
#include "..\Display.h"
#include "..\Encoders.h"
#include "..\Filter.h"
#include "..\InfoBox.h"
#include "..\Menu.h"
#include "..\MenuProc.h"
#include "si5351.h" // modified https://github.com/etherkit/Si5351Arduino
#include "..\Tune.h"
#include "..\Utility.h"

//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

int TxRxFreq, NCOFreq;

bool splitVFO;

int CWFreqShift = 750;
int calFreqShift = 0;

Si5351 si5351;

//-------------------------------------------------------------------------------------------------------------
// Code
//-------------------------------------------------------------------------------------------------------------

FLASHMEM void InitSI5351() {
  si5351.reset();
  si5351.init(SI5351_CRYSTAL_LOAD_10PF, Si_5351_crystal, freqCorrectionFactor);
  si5351.set_ms_source(SI5351_CLK2, SI5351_PLLB); //  Allows CLK1 and CLK2 to exceed 100 MHz simultaneously.
  si5351.drive_strength(SI5351_CLK1, SI5351_DRIVE_8MA);
  si5351.drive_strength(SI5351_CLK2, SI5351_DRIVE_8MA);
}

void SetSI5351FreqCorFactor(int factor) {
  si5351.init(SI5351_CRYSTAL_LOAD_10PF, Si_5351_crystal, factor);
}

/*****
  Purpose: Set center tuning frequency
           NCOFreq is unchanged
*****/
void SetTxRxFreq(int freq) {
  TxRxFreq = freq;

  SetFreq();

  switch(displayState) {
    case DISPLAY_T41:
      ShowFrequency();          // update frequency display
      ShowOperatingStats();     // update center frequency in band info
      ShowSpectrumFreqValues(); // update spectrum frequency values
      break;

    case DISPLAY_BEACON_MONITOR:
      break;

    case DISPLAY_CALIBRATION:
      ShowOperatingStats();

      if(calibrateFlag == 1) {
        // receive IQ calibration
        ShowSpectrumFreqValues();
      }
      break;

    case DISPLAY_FULL_MENU:
      ShowFrequency();
      ShowOperatingStats();
      break;

    default:
    // no screen updates at all
    break;
  }
}

/*****
  Purpose: Reset tuning to center
           NCOFreq is set to zero

  Parameter list:
  void

  Return value:
  void
*****/
void ResetTuning() {
  centerFreq += NCOFreq;
  NCOFreq = 0L;

  SetTxRxFreq(centerFreq);

  DrawBandwidthBar();
}

/*****
  Purpose: Adjust center tuning frequency
           NCOFreq is unchanged

  Parameter list:
    long tuneChange - amound to change center freq
*****/
void SetCenterTune(int tuneChange) {
  centerFreq += tuneChange;  // tune the master vfo

  SetTxRxFreq(centerFreq + NCOFreq);
}

/*****
  Purpose: Set NCO frequency
*****/
void SetNCOFreq(int newNCOFreq) {
  int lowSideAdj = 0, highSideAdj = 0;

  switch(bands[currentBand].demod) {
    case DEMOD_USB:
    case DEMOD_PSK31_WAV:
    case DEMOD_PSK31:
    case DEMOD_FT8:
    case DEMOD_FT8_WAV:
      lowSideAdj = 0;
      highSideAdj = currentFilterHiCut;
      break;

    case DEMOD_LSB:
      lowSideAdj = currentFilterHiCut;
      highSideAdj = 0;
      break;

    case DEMOD_AM:
    case DEMOD_SAM:
      break;

    case DEMOD_NFM:
      break;
  }

  NCOFreq = newNCOFreq;
  fineTuneFlag = true;
  if(activeVFO == VFO_A) {
    currentFreqA = centerFreq + NCOFreq;
  } else {
    currentFreqB = centerFreq + NCOFreq;
  }

  // recenter at band edges
  if(spectrumZoom != 0) {
    if((NCOFreq + highSideAdj) >= (96000 / (1 << spectrumZoom))) {
      NCOFreq += highSideAdj;
      fineTuneFlag = false;
      resetTuningFlag = true;
      return;
    }
    if((NCOFreq - lowSideAdj) <= (-96000 / (1 << spectrumZoom))) {
      NCOFreq -= lowSideAdj;
      fineTuneFlag = false;
      resetTuningFlag = true;
      return;
    }
  } else if(NCOFreq > 142000 || NCOFreq < -43000) {  // Offset tuning window in zoom 1x
    fineTuneFlag = false;
    resetTuningFlag = true;
    return;
  }

  TxRxFreq = centerFreq + NCOFreq;
}

/*****
  Purpose: Set fine tuning frequency
*****/
void SetFineTune(int tuneChange) {
  SetNCOFreq(NCOFreq + tuneChange);
}

int EvenDivisor(long freq2) {
  int divisor = 126;

  // next 6 ifs added by DRM VK3KQT for use by a phase method of time delay described by
  // https://tj-lab.org/2020/08/27/si5351単体で3mhz以下の直交信号を出力する/
  // for below 3.2MHz the ~limit of PLLA @ 400MHz for a 126 divider
  if(freq2 < 100000)
      divisor = 8192;

  if((freq2 >= 100000) && (freq2 < 200000))   // PLLA 409.6 MHz to 819.2 MHz
     divisor = 4096;

  if((freq2 >= 200000) && (freq2 < 400000))   //   ""          ""
     divisor = 2048;

  if((freq2 >= 400000) && (freq2 < 800000))   //    ""          ""
     divisor = 1024;

  if((freq2 >= 800000) && (freq2 < 1600000))   //    ""         ""
     divisor = 512;

  if((freq2 >= 1600000) && (freq2 < 3200000))   //    ""        ""
     divisor = 256;
   //==================================================================
  // the original divisor
  // if(freq2 < 6850000)
  if((freq2 >= 3200000) && (freq2 < 6850000))   // 403.2 MHz - 863.1 MHz
    divisor = 126;

  if((freq2 >= 6850000) && (freq2 < 9500000))
    divisor = 88;

  if((freq2 >= 9500000) && (freq2 < 13600000))
    divisor = 64;

  if((freq2 >= 13600000) && (freq2 < 17500000))
    divisor = 44;

  if((freq2 >= 17500000) && (freq2 < 25000000))
    divisor = 34;

  if((freq2 >= 25000000) && (freq2 < 36000000))
    divisor = 24;

  if((freq2 >= 36000000) && (freq2 < 45000000))
    divisor = 18;

  if((freq2 >= 45000000) && (freq2 < 60000000))
    divisor = 14;

  if((freq2 >= 60000000) && (freq2 < 80000000))
    divisor = 10;

  if((freq2 >= 80000000) && (freq2 < 100000000))
    divisor = 8;

  if((freq2 >= 100000000) && (freq2 < 150000000)) // G0ORX changed upper limit
    divisor = 6;

  if((freq2 >= 150000000) && (freq2 < 220000000))
    divisor = 4;

  // ? G0ORX - for higher bands
  if(freq2>=220000000) {
    divisor = 2;
  }
  return divisor;
}

/*****
  Purpose: Set si5351 frequency

  CAUTION: SI5351_FREQ_MULT is set in the si5253.h header file and is 100UL
*****/
void SetFreq(bool reset) {
  unsigned long long Clk1SetFreq;
  long long f = centerFreq;
  long long freq, pll_freq;
  static int multiple = 126;
  static int oldMultiple = 0;

  // NEVER USE AUDIONOINTERRUPTS HERE: that introduces annoying clicking noise with every frequency change

  Clk1SetFreq = ((f * SI5351_FREQ_MULT) + 48000.0 * SI5351_FREQ_MULT);
  multiple = EvenDivisor(Clk1SetFreq / SI5351_FREQ_MULT);
  pll_freq = Clk1SetFreq * multiple;
  freq = pll_freq / multiple;     // is this equal to Clk1SetFreq?

  if((multiple == oldMultiple) && !reset) {                // Still within the same multiple range
    si5351.set_pll(pll_freq, SI5351_PLLA);      // just change PLLA on each frequency change of encoder
                                                // this minimizes I2C data for each frequency change within a
                                                // multiple range
  } else if(multiple <= 126) {                                 // this the library setting of phase for freqs
    si5351.set_freq_manual(freq, pll_freq, SI5351_CLK0);  // greater than 3.2MHz where multiple is <= 126
    si5351.set_freq_manual(freq, pll_freq, SI5351_CLK1);   // set both clocks to new frequency
    si5351.set_phase(SI5351_CLK0, 0);                      // CLK0 phase = 0
    si5351.set_phase(SI5351_CLK1, multiple);               // Clk1 phase = multiple for 90 degrees(digital delay)
    si5351.pll_reset(SI5351_PLLA);                         // reset PLLA to align outputs
    si5351.output_enable(SI5351_CLK0, 1);                  // set outputs on or off
    si5351.output_enable(SI5351_CLK1, 1);
    //si5351.output_enable(SI5351_CLK2, 0);
  } else {        // this is the timed delay technique for frequencies below 3.2MHz as detailed in
                  // https://tj-lab.org/2020/08/27/si5351単体で3mhz以下の直交信号を出力する/
    cli();                //__disable_irq(); or __enable_irq();     // or cli()/sei() pair; needed to get accurate timing??
    //si5351.output_enable(SI5351_CLK0, 0);  // optional switch off clocks if audio effects are generated
    //si5351.output_enable(SI5351_CLK1, 0);  //  with the change of multiple below 3.2MHz
    si5351.set_freq_manual((freq - 400ULL), pll_freq, SI5351_CLK0);  // set up frequencies of CLK 0/1 4 Hz low
    si5351.set_freq_manual((freq - 400ULL), pll_freq, SI5351_CLK1);  // as per TJ-Labs article
    si5351.set_phase(SI5351_CLK0, 0);                          // set phase registers to 0 just to be sure
    si5351.set_phase(SI5351_CLK1, 0);
    si5351.pll_reset(SI5351_PLLA);                             // align both clockss in phase
    si5351.set_freq_manual(freq, pll_freq, SI5351_CLK0);       // set clock 0  to required freq
    //delayNanoseconds(625000000);       // 62.5 * 1000000      //configured for a 62.5 mSec delay at 4 Hz difference
    delayMicroseconds(58500);                       //nominally 62500 this figure can be adjusted for a more exact delay which is phase
    si5351.set_freq_manual(freq, pll_freq, SI5351_CLK1);       // set CLK 1 to the required freq after delay
    sei();
    si5351.output_enable(SI5351_CLK0, 1);                      // switch them on to be sure
    si5351.output_enable(SI5351_CLK1, 1);                      //    ""        ""
    //si5351.output_enable(SI5351_CLK2, 0);
  }
  oldMultiple = multiple;
}

FLASHMEM void SplitVFOFollowup() {
  // *** TODO: need to reestablish "Split Active" that didn't work in ver49.2k ***
  tft.setTextColor(RA8875_RED);
  tft.setCursor(FILTER_PARAMETERS_X + 180, FILTER_PARAMETERS_Y + 6);
  tft.print("Split Active");
  splitVFO = true;
}

/*****
  Purpose: Set VFO A to receive frequency and VFO B to the transmit frequency
*****/
FLASHMEM void DoSplitVFO() {
  currentFreqB = currentFreqA;

  // GetMenuValue(minValue, maxValue, startValue, increment, prompt, valueOffset)
  GetMenuValue(-40, 30, &currentFreqB, SPLIT_INCREMENT, "Xmit offset:", 200, NULL, NULL, &SplitVFOFollowup);
}
