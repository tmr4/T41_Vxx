
#include "catControl.h"

//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

//-------------------------------------------------------------------------------------------------------------
// Code
//-------------------------------------------------------------------------------------------------------------

// band down
// "BD;" (length 3) or "BDx;" (length 4)
void CatControl::handleBandDown(const char* cmd, const size_t len) {
  if(len == 3) { // Kenwood TS-2000
    ChangeBand(-1);
    // SendAS(); // PC control specific ???
  } else if(len == 4) { // hybrid
    ChangeBand(t41.ActiveBand - atoi(&cmd[2]), false);
  }
}

// band up
// "BU;" (length 3) or "BUx;" (length 4)
void CatControl::handleBandUp(const char* cmd, const size_t len) {
  if(len == 3) { // Kenwood TS-2000
    ChangeBand(1);
    // SendAS(); // PC control specific ???
  } else if(len == 4) { // hybrid
    ChangeBand(t41.ActiveBand - atoi(&cmd[2]), false);
  }
}

// read/set VFO A frequency
// "FA;" (length 3) or "FAxxxxxxxxxxx;" (length 14)
void CatControl::handleFA(const char* cmd, const size_t len) {
  if(len == 3) { // read
    snprintf(msg, sizeof(msg), "FA%011d;", (int)t41.GetFreqA());
    send(msg);
  } else if(len == 14) { // set
    long f = atol(&cmd[2]);

    ChangeBand(f);
    if(t41.MouseCenterTuneActive) {
      t41.SetFreqA(f);
    } else {
      t41.NCOFreq.Update(f); // *** TODO: verify, this should only happen on active VFO ***
    }
  }
}

// read/set VFO B frequency
// "FB;" (length 3) or "FBxxxxxxxxxxx;" (length 14)
void CatControl::handleFB(const char* cmd, const size_t len) {
  if(len == 3) { // read
    snprintf(msg, sizeof(msg), "FB%011;", (int)t41.GetFreqB());
    send(msg);
  } else if(len == 14) { // set
    long f = atol(&cmd[2]);

    ChangeBand(f);
    if(t41.MouseCenterTuneActive) {
      t41.SetFreqB(f);
    } else {
      t41.NCOFreq.Update(f); // *** TODO: verify, this should only happen on active VFO ***
    }
  }
}

// read/set current VFO center frequency
// "FC;" (length 3) or "FCxxxxxxxxxxx;" (length 14)
void CatControl::handleFC(const char* cmd, const size_t len) {
  if(len == 3) { // read
    snprintf(msg, sizeof(msg), "FC%011d;", (int)t41.CenterFreq);
    send(msg);
  } else if(len == 14) { // set
    long f = atol(&cmd[2]);
    t41.CenterFreq.Update(f);
    SetFreq(f);
  }
}

// read radio ID
// "ID;" (length 3) or "IDxxx;" (length 6)
// Kenwood TS-890S: ID024; // *** WSJT-X expects this even when TS-2000 is selected ***
// Kenwood TS-2000: ID019;
void CatControl::handleID(const char* cmd, const size_t len) {
  if(len == 3 && cmd[2] == ';') {
    snprintf(msg, sizeof(msg), "ID%03d;", t41.RadioID);
    send(msg);
  } else if(len == 6 && cmd[5] == ';') {
    ackIdReceipt();
  }
}
