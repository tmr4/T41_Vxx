
#include <TimeLib.h>                   // Part of Teensy Time library

#include "SDT.h"

#include <QNEthernet.h>
using namespace qindesign::network;
#include "input_tcp.h"
#include "output_tcp.h"

#include "ButtonProc.h"
#include "Tune.h"

#include "debug.h"

#include "radios.h"

//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

#if REC_IQ_FROM_T41_ETHER || SEND_IQ_TO_REMOTE_ETHER
EthernetClient ethernetControl;
RemoteRadio radio;
#endif
#if SEND_IQ_TO_REMOTE_ETHER
EthernetServer ethernetServerControl(80);
#endif

//-------------------------------------------------------------------------------------------------------------
// Code
//-------------------------------------------------------------------------------------------------------------

void T41ControlSetup() {
#if REC_IQ_FROM_T41_ETHER || SEND_IQ_TO_REMOTE_ETHER
  const IPAddress clientIP{192, 168, 1, 101};
  const IPAddress subnet{255, 255, 255, 0};
  const IPAddress gateway{192, 168, 1, 1};
  const IPAddress serverIP{192, 168, 1, 100};
#if REC_IQ_FROM_T41_ETHER
  // Remote Ethernet Client
  InitEthernet(clientIP, subnet, gateway);
#elif SEND_IQ_TO_REMOTE_ETHER
  // T41 Ethernet Server
  InitEthernet(serverIP, subnet, gateway);
  ethernetServerControl.begin(80);
#endif
  ethernetControl.setConnectionTimeoutEnabled(false);
  ethernetControl.setNoDelay(true);
  radio.setLink(ethernetControl);
#endif
}


// band down
// "BD;" (length 3) or "BDx;" (length 4)
void CatControl::handleBD(const char* cmd, bool isRead) {
  if(isRead) {
    ChangeBand(-1);
    // SendAS(); // PC control specific ???
  } else {
    ChangeBand(t41.ActiveBand - atoi(&cmd[2]), false);
  }
}

// band up
// "BU;" (length 3) or "BUx;" (length 4)
void CatControl::handleBU(const char* cmd, bool isRead) {
  if(isRead) {
    ChangeBand(1);
    // SendAS(); // PC control specific ???
  } else {
    ChangeBand(t41.ActiveBand - atoi(&cmd[2]), false);
  }
}

// read/set VFO A frequency
// "FA;" (length 3) or "FAxxxxxxxxxxx;" (length 14)
void CatControl::handleFA(const char* cmd, bool isRead) {
  if(isRead) {
    snprintf(msg, sizeof(msg), "FA%011d;", (int)t41.GetFreqA());
  } else {
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
void CatControl::handleFB(const char* cmd, bool isRead) {
  if(isRead) {
    snprintf(msg, sizeof(msg), "FB%011d;", (int)t41.GetFreqB());
  } else {
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
void CatControl::handleFC(const char* cmd, bool isRead) {
  if(isRead) {
    snprintf(msg, sizeof(msg), "FC%011d;", (int)t41.CenterFreq);
  } else {
    long f = atol(&cmd[2]);
    t41.CenterFreq.Update(f);
    SetFreq(f);
  }
}

// read radio ID
// "ID;" (length 3) or "IDxxx;" (length 6)
// Kenwood TS-890S: ID024; // *** WSJT-X expects this even when TS-2000 is selected ***
// Kenwood TS-2000: ID019;
void CatControl::handleID(const char* cmd, bool isRead) {
  if(isRead) {
    snprintf(msg, sizeof(msg), "ID%03d;", (int)t41.RadioID);
  } else {
    ackIdReceipt();
  }
}

// set Teensy RTC
// "TMxxxxxxxxxxx;" (length 14)
void CatControl::handleTM(const char* cmd, bool isRead) {
  if(!isRead) {
    Teensy3Clock.set(atol(&cmd[2]));
    setTime(atol(&cmd[2]));
  }
}
