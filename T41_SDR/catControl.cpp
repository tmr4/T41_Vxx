

#include "SDT.h"

#include "ButtonProc.h"
#include "hardware.h"
#include "Utility.h"

#include "catControl.h"
#include "connectManager.h"

#include "debug.h"

//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

extern ConnectManager connectManager;
extern T41Properties t41;

#include "src\displayRA8875\RA8875\src\RA8875.h"
extern RA8875 tft;

//-------------------------------------------------------------------------------------------------------------
// Code
//-------------------------------------------------------------------------------------------------------------

void CatControl::processCommand(const char* cmd) {
  if(enabled) {
    // convert the 2 character command code into its CAT table index
    uint8_t catHash = CatToken2Hash((uint16_t)((cmd[0] << 8) | cmd[1]));
    const CATCommand* item;

    item = catHash >= 128 ? nullptr : catCommands[catHash];

    if(item) {
      // CAT command found
      if(item->lenR != 0 && cmd[item->lenR-1] == ';') {
        //Serial.printf("Received read: %s\n", cmd);
        //if(useWSJT) {
        //  // *** spy WSJT-X commands in infobox ***
        //  static int row = 340;
        //  static int col = 540;
        //  tft.setCursor(col, row);
        //  tft.print(cmd);
        //  col += 8 * 4;
        //  if(col > 795) { col = 540; row += 20; }
        //}

        // read command properly formed
        const T41Update* ptr = item->readProperty;

        if(ptr) {
          int value = item->readProperty->getValue();
          snprintf(msg, sizeof(msg), item->format, value);
          send(msg);
        } else {
          HandleNonstandardProperty(item);
        }
      } else if(item->lenS != 0 && cmd[item->lenS-1] == ';') {
        //Serial.printf("Received set: %s\n", cmd);
        //if(useWSJT) {
        //  // *** spy WSJT-X commands in infobox ***
        //  static int row = 340;
        //  static int col = 540;
        //  tft.setCursor(col, row);
        //  tft.print(cmd);
        //  col += 8 * 14;
        //  if(col > 785) { col = 540; row += 20; }
        //}

        // set command properly formed
        item->action->execute(this, cmd);
      } else {
        // command not properly formed
        // *** TODO: consider sending followup if command not properly formed
        //if(useWSJT) {
        //  // *** spy WSJT-X commands in infobox ***
        //  static int row = 340;
        //  static int col = 540;
        //  tft.setCursor(col, row);
        //  tft.print(cmd);
        //  col += 8 * 3;
        //  if(col > 785) { col = 540; row += 20; }
        //}

        return;
      }
    } else {
      // *** TODO: consider sending ?; if command not recognized
      //Serial.printf("bad item: %s, %d\n", cmd, catHash);
    }
  }
}

void SendCommand(int id) {
  //catControl.notifyRemote(id);
}

// handle answer CAT command for undefined properties nonstandard answer formats
void CatControl::HandleNonstandardProperty(const CATCommand* item) {
  uint16_t token = item->token;
  int value = 0;
  int vfo, freq;

  switch(token) {
    case "FA"_cat:
      value = t41.GetFreqA();
      break;
    case "FB"_cat:
      value = t41.GetFreqB();
      break;
    case "ID"_cat:
      if(useWSJT) {
        // Kenwood TS-890S: ID024; // *** WSJT-X expects this even when TS-2000 is selected ***
        // Kenwood TS-2000: ID019;
        value = 24;
      } else {
        if(connectManager.isRemote()) heartbeat = millis(); // note time for heartbeat
        value = t41.RadioID;
      }
      break;
    case "IF"_cat:
      if(useWSJT) {
        // *** TODO: verify that this is latest WSJT-X IF; format ***
        //
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
      return;
      break;
    case "SF"_cat: // WSJT-X
      vfo = atoi(&cmd[2]);
      freq = vfo == 0 ? t41.GetFreqA() : t41.GetFreqB();
      snprintf(msg, sizeof(msg), item->format, vfo, freq, 2);
      send(msg);
      return;
      break;
    case "SM"_cat:
      value = (int)CalcSignalStrength()*10;
      break;
  }
  snprintf(msg, sizeof(msg), item->format, value);
  send(msg);
}
