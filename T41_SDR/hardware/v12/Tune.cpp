// v12 specific hardware file

#include <si5351.h> // https://github.com/tmr4/Si5351_T41

#include "..\SDT.h"

#include "..\Tune.h"

//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

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

  Clk1SetFreq = ((f * SI5351_FREQ_MULT) + ((int)intermediateFreq) * SI5351_FREQ_MULT);
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
