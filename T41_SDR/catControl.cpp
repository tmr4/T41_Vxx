

#include "SDT.h"

#include "ButtonProc.h"
#include "hardware.h"
#include "Utility.h"

#include "radio.h"
#include "connectManager.h"

//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

extern ConnectManager transport;
extern RemoteRadio radio;
//RemoteRadio radio(&catCommands, &wsjtCommands);
extern T41Properties t41;

//-------------------------------------------------------------------------------------------------------------
// Code
//-------------------------------------------------------------------------------------------------------------

void SendCommand(int id) {
  radio.notifyRemote(id);
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

// get read CAT command value or otherwise perform non-standard action on a read command
// *** TODO: got to be a better way ***
int CatControl::GetPropertyValue(int token) {
  int value = 0;
  int vfo, freq;

  switch(token) {
    case "AI"_cat: // WSJT-X
      snprintf(msg, sizeof(msg), "AI0;"); // Auto info off
      send(msg);
      break;
    case "BD"_cat:
      ChangeBand(-1);
      // SendAS(); // PC control specific ???
      break;
    case "BU"_cat:
      ChangeBand(1);
      // SendAS(); // PC control specific ???
      break;
    case "FA"_cat:
      value = t41.GetFreqA();
      break;
    case "FB"_cat:
      value = t41.GetFreqB();
      break;
    case "FC"_cat:
      value = t41.CenterFreq;
      break;
    case "FF"_cat:
      value = t41.NCOFreq;
      break;
    case "FS"_cat:
      value = !t41.MouseCenterTuneActive;
      break;
    case "F0"_cat:
      value = t41.CenterTuneIndex;
      break;
    case "F1"_cat:
      value = t41.FineTuneIndex;
      break;
    case "FT"_cat: // WSJT-X
      snprintf(msg, sizeof(msg), "FT0;"); // T41 always responds transmit on VFO A
      send(msg);
      break;
    case "GT"_cat:
      value = t41.AGCMode;
      break;
    case "ID"_cat:
      if(useWSJT) {
        // Kenwood TS-890S: ID024; // *** WSJT-X expects this even when TS-2000 is selected ***
        // Kenwood TS-2000: ID019;
        value = 24;
      } else {
        if(transport.isRemote()) heatbeart = millis(); // note time for heartbeat
        value = t41.RadioID;
      }
      break;
    case "IF"_cat:
      if(useWSJT) {
        // WSJT-X recieved w/ USB Serial+Audio: IF00007048000125004+0000000001000361100007030000; which is 48, expects 37
        //                            1         2         3      |  4
        //                  0123456789012345678901234567890123456789012345678
        //                  IF00007048000125004+0000000001000361100007030000;
        // should be        IF000070480005000+00000000001xx000000;
        //
        // should have sent per below:
        //                            1         2         3      |  4
        //                  0123456789012345678901234567890123456789012345678
        //                  IF000070480005000+00000000001xx000000;
        //                    01234567890
        // TxRxFreq %011d     00007048000
        // 5000     %04d                 5000
        // 0        %+06d                    +00000
        // 0        %d                             0
        // 0        %d                              0
        // 0        %d                               0
        // 0        %02d                              00
        // XRState  %d                                  1
        // mode     %d                                   x
        // aVFO     %d                                    x
        // 0        %d                                     0
        // 0        %d                                      0
        // 0        %d                                       0
        // 0        %02d                                      00
        // 0        %d                                          0
        // ;                                                     ;
        //                  IF000070480005000+00000000001xx000000;
        snprintf(msg, sizeof(msg), "IF%011d%04d%+06d%d%d%d%02d%d%d%d%d%d%d%02d%d;",
          t41.ActiveFreq(),     // freq in Hz
          5000,         // freq step size
          0,            // RIT/XIT freq in Hz, +-99999, this isn't preserved in the T41 but would be VFO A - VFO B if split
          0,            // RIT on/off
          0,            // XIT on/off
          0,0,          // channel bank number
          !GetXRState(),     // RX/TX (1/0)
          2,            // operating mode
          (int)t41.ActiveVFO,    // RX VFO
          0,            // scan Status
          0,            // split status (Kenwood manual refers to SP command which doesn't exist)
          0,            // CTCSS enabled
          1,            // CTCSS tone frequency
          0             // shift status
        );
      } else {
        // *** Warning: this is not the Kenwood implimentation ***
        sprintf(msg, "IF%011d%d%d%d%03d%+06d%04d%d%d%d%d%d%d%d%d%011d;",
          // active VFO Freq = TxRxFreq, t41.CenterFreq = TxRxFreq - NCOFreq
          //  *** TODO: we only need 8 digits for first field for T41, consider using other 3 for something ***
          t41.ActiveFreq(), // freq in Hz (%011d) at index 2
          (int)t41.ActiveBand,            // current band (%d) at index 13
          (int)t41.RadioMode,             // transmission mode (%d) at index 14
          (int)t41.DemodMode,             // demodulation mode (%d)  at index 15
          (int)t41.AudioVolume,           // audio volume (%03d) at index 16
          (int)t41.NCOFreq,               // NCO freq (%+06d) at index 19
          (int)t41.NoiseFloor,            // noise floor (%04d) at index 25 *** TODO: verify need for +- or number of digits ***
          (int)t41.LiveNoiseFloor,        // set noise floor active/inactive 1/0 (%d) at index 29
          !GetXRState(),                  // RX/TX (1/0) (%d) at index 30
          (int)t41.ActiveVFO,             // VFO A/B (0/1) (%d) at index 31
          (int)t41.MouseCenterTuneActive, // fine or center tune enabled (0/1) (%d) at index 32
          (int)t41.FineTuneIndex,         // fine tune index (%d) at index 33
          (int)t41.CenterTuneIndex,       // center tune index (%d) at index 34
          (int)t41.AGCMode,               // AGC mode (%d) at index 35
          (int)t41.SpectrumZoom,          // spectrum zoom (%d) at index 36
          (int)t41.InactiveFreq           // inactive VFO freq in Hz (%011d) at index 37
          //splitVFO ? 1 : 0,             // VFO split status (%d) at index xx
        );
      }
      send(msg);
      break;
    case "KS"_cat:
      value = DEFAULT_KEYER_WPM;
      break;
    case "MD"_cat:
      value = t41.DemodMode;
      break;
    case "ME"_cat:
      value = t41.RadioMode;
      break;
    case "NF"_cat:
      value = t41.NoiseFloor;
      break;
    case "NG"_cat:
      value = t41.LiveNoiseFloor;
      break;
    case "NH"_cat:
      value = t41.FilterHiCut;
      break;
    case "NL"_cat:
      value = t41.FilterLoCut;
      break;
    case "N1"_cat:
      value = t41.NoiseFilter;
      break;
    case "PC"_cat:
      value = t41.TxPower;
      break;
    case "PG"_cat:
      value = t41.RFGain;
      break;
    case "SF"_cat: // WSJT-X
      vfo = atoi(&cmd[2]);
      freq = vfo == 0 ? t41.GetFreqA() : t41.GetFreqB();
      snprintf(msg, sizeof(msg), "SF%d%011d%d;", vfo, freq, 2);
      send(msg);
      break;
    case "SM"_cat:
      value = (int)CalcSignalStrength()*10;
      break;
    case "VO"_cat:
      value = t41.AudioVolume;
      break;
    case "ZM"_cat:
      value = t41.SpectrumZoom;
      break;
  }
  return value;
}
