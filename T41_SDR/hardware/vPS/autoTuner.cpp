#include "..\SDT.h"
#include "..\CW_Excite.h"

/*
Allows auto tuning with the LDG AT-100ProII Autotuner
Requires the Y-ACC-1 cable (Radio end (Red) Ring connected to Tuner end (Black) Tip)
Use external pullup on radio ring to +3.3V, 100nF to ground
*/

// LDG_AUTO_TUNER defined in hardware.h

#ifdef LDG_AUTO_TUNER
void InitTuner() {
  pinMode(LDG_AUTO_TUNER, INPUT);
}

void AutoTune() {
  int pwr = t41.TxPower;
  int delay = t41.CWTransmitDelay;

  // the LDG AT-100ProII Autotuner can tune with 1W
  // *** TODO: reset to lower level once CW pwr is calibrated ***
  t41.TxPower = 4;
  // we don't need a long delay
  t41.CWTransmitDelay = 10;

  CWTransmit(LDG_AUTO_TUNER);

  // restore TX power and delay
  t41.TxPower = pwr;
  t41.CWTransmitDelay = delay;
}
#else
void InitTuner() {}
void AutoTune() {}
#endif
