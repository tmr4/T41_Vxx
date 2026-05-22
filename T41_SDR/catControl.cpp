
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

RemoteRadio radio;

//-------------------------------------------------------------------------------------------------------------
// Code
//-------------------------------------------------------------------------------------------------------------

void SendCommand(int id) {
  radio.notifyRemote(id);
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
// "ID;" (length 3), Answer: "IDxxx;" (length 6)
// *** ackIdReceipt is provided to acknowledge receipt of a properly formated reply ***
void CatControl::handleID(const char* cmd, bool isRead) {
  if(isRead) {
    snprintf(msg, sizeof(msg), "ID%03d;", (int)t41.RadioID);
  } else {
    ackIdReceipt();
  }
  heatbeart = millis(); // note time for heartbeat
}

// set Teensy RTC
// "TMxxxxxxxxxxx;" (length 14)
void CatControl::handleTM(const char* cmd, bool isRead) {
  if(!isRead) {
    Teensy3Clock.set(atol(&cmd[2]));
    setTime(atol(&cmd[2]));
  }
}

// CAT command prefix for T41 display items and properties
// *** simplifies notifying remote unit of property changes ***
// *** list needed to be in order of id for radio elements in SDT.h
// *** as those labels are used to identify the item ***
const uint16_t CatControl::catItems[T41_ITEMS] {
  "VO"_cat,   // T41_ITEM_VOL          0
  "GT"_cat,   // T41_ITEM_AGC          1
  "F0"_cat,   // T41_ITEM_TUNE         2
  "F1"_cat,   // T41_ITEM_FINE         3
  "ZM"_cat,   // T41_ITEM_ZOOM         4
  "NG"_cat,   // T41_ITEM_FLOOR        5
  "xx"_cat,   // T41_ITEM_NOTCH        6
  "N1"_cat,   // T41_ITEM_FILTER       7
  "xx"_cat,   // T41_ITEM_COMPRESS     8
  "PG"_cat,   // T41_ITEM_RFGAIN       9
  "xx"_cat,   // T41_ITEM_EQUALIZER    10
  "xx"_cat,   // T41_ITEM_DECODER      11
  "xx"_cat,   // T41_ITEM_KEY          12
  "xx"_cat,   // T41_ITEM_KEYER        13
  "xx"_cat,   // T41_ITEM_FT8          14
  "xx"_cat,   // T41_ITEM_FT8_INT      15
  "xx"_cat,   // T41_ITEM_FT8_TX       16
  "xx"_cat,   // T41_ITEM_FT8_CQ       17
  "xx"_cat,   // T41_ITEM_FT8_TXF      18
  "xx"_cat,   // T41_ITEM_FT8_RXF      19
  "xx"_cat,   // T41_ITEM_STACK        20
  "xx"_cat,   // T41_ITEM_HEAP         21
  "xx"_cat,   // T41_ITEM_TEMP         22
  "xx"_cat,   // T41_ITEM_LOAD         23
  "FS"_cat,   // T41_ITEM_MOUSE        24 // MouseCenterTuneActive
  "NF"_cat,   // T41_ITEM_NOISE        25 // NoiseFloor
  "ME"_cat,   // T41_ITEM_RADIO_MODE   26
  "MD"_cat,   // T41_ITEM_DEMOD_MODE   27
  "BD"_cat,   // T41_ITEM_BAND         28 // ActiveBand
  "PC"_cat,   // T41_ITEM_POWER        29 // TxPower
  "FC"_cat,   // T41_ITEM_FREQ         30 // CenterFreq
  "FF"_cat,   // T41_ITEM_NCO          31 // NCOFreq
  "NH"_cat,   // T41_ITEM_FHI          32 // FilterHiCut
  "NL"_cat,   // T41_ITEM_FLO          33 // FilterLoCut
  "xx"_cat,   // T41_ITEM_SCALE        34 // FreqSpecScale
  "xx"_cat,   // T41_ITEM_CW_FILTER    35 // CWFilterIndex
  "xx"_cat    // T41_ITEM_CW_DECODER   36
};

//void CatControl::send(const char *msg) {
//  link->print(msg);
//  //ethernetControl.flush();
//}
