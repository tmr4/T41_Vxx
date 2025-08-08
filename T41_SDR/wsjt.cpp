
// wsjt.cpp impliments CAT control for WSJT-X.  It models the very limited subset of CAT commands used by WSJT-X for the Kenwood TS-890-S.
// Note that this module reflects the WSJT-X implimentation, not Kenwood's.  It's actually closer to the Kenwood TS-2000.
// This module isn't appropriate if you need true TS-890S CAT control.
//
// This module can also be used with the T41Sever app with the DX Lab Suite Commander selected as the rig.
// DX Lab impliments a very limited groups of commands: .
//
// Reference:
// https://www.kenwood.com/i/products/info/amateur/pdf/ts890_pc_command_en_rev1.pdf
// https://www.qrzcq.com/pub/RADIO_MANUALS/KENWOOD/KENWOOD--TS-2000-User-Manual.pdf
//
// WSJT-X starts by issuing the following commands with TS-890S selected as the rig:
//  ID; PS; IF; AI; KS; FA; FT; TB; SF0; IF; FA000xxxxx055; FA; FA; FA; FA000xxxxx000; IF; FT; TB;
//
// WSJT-X does a periodic poll which issues the following commands in order and expects the normal answer:
//  w/ TS-890S rig: FT; TB; FA; SF0; (sometime IF; is used in place of FT;)
//  w/ T41Server: FA; SP; MD;
//
// When band/freq change is selected in WSJT-X: FA; FA000xxxxxxxx; ID; SF0;
/*********************************************************************************************************
   This module impliments the following CAT commands:
    S=set, R=read, A=answer

    AI - Auto Information (SRA)
      AIx; - ignored, but haven't observed wsjt-x using
      AI;
      AI0; AI off, *** TODO: consider implimenting auto information which informs wsjt-x of radio state changes ***

    FA - VFO A Frequency (SRA)
      FAxxxxxxxxxxx;
      FA;
      FAxxxxxxxxxxx;

      Notes: FA must handle band changes as WSJT-X doesn't model band change logic.

    FB - VFO B Frequency (SRA)
      FBxxxxxxxxxxx;
      FB;
      FBxxxxxxxxxxx;

      Notes: FB must handle band changes as WSJT-X doesn't model band change logic.

    FT - Transmitter Function (VFO A / VFO B) (SRA)
      FTx;
      FT;
      FT0; T41 always responds with VFO A

    ID - VFO A Frequency (RA)
      ID;
      ID024; T41 always responds with TS-890S id

    KS - Keying Speed (SRA)
      KSxxx; - not implimented
      KS;
      KS015; fixed for now

    MD - Operating Mode Status (TS-2000 command, not TS890-S) (SRA)
      MDx; 2:USB
      MD;
      MD2; T41 FT8 mode is always USB

    PS - Power ON/OFF (RA) I haven't observed WSJT-X using the set form of this command
      PS;
      PS1; T41 always responds with power on

    RX - Receive Function State (S) an answer is only made when AI mode is on
      RX;

    SF - Sets and Reads the VFO (SRA)
      SFxyyyyyyyyyyyz;
      SFx; 0=VFO A; 1=VFO B
      SF0; T41 always returns no split mode operation

    SP - Split Operation Frequency Setting (SRA)
      SPx;
      SP;
      SP0; T41 always returns no split mode operation

    TB - Split
      TBx;
      TB;
      TB0; T41 always responds split off

    TX - Transmission Mode (S) an answer is only made with AI mode on
      TXx; 0=PTT if x isn't specified it is set to 0 (WSJT-X uses TX; form)

*********************************************************************************************************/

// Using this module:

#include <TimeLib.h>                   // Part of Teensy Time library

#include "SDT.h"

#include "ButtonProc.h"
#include "debugSerial.h"
#include "Display.h"
#include "Encoders.h"
#include "EEPROM.h"
#include "keyboard.h"
#include "InfoBox.h"
#include "MenuProc.h"
#include "mouse.h"
#include "Tune.h"
#include "Utility.h"

//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

bool ft8PTT = false;

//-------------------------------------------------------------------------------------------------------------
// Code
//-------------------------------------------------------------------------------------------------------------

// the three usb serial objects in the teensy (Serial, wsjtSerial and SerialUSB2) are all different classes (usb_serial_class, usb_serial2_class, and usb_serial3_class)
// I suppose to prevent naming conflict somewhere, but this prevents having serial commands with a common argument specifying the serial channel to use, such as
// void WSJTControlSetup(Stream& serial) { serial.begin(); }.  As such might as well duplicate these functions for both the T41 control app and Beacon monitor
// *** currently must be in FT8 data mode for WSJT-X to connect ***
// *** TODO: work up FT8 init so WSJT-X can get into data mode ***
void WSJTControlSetup() {
  //wsjtSerial.begin(19200); // this is unneeded for Teensy (https://www.pjrc.com/teensy/td_serial.html) meaning the setup call is unneeded as well
}

void WSJTControlSendCmd(char *cmd) {
  int sizeBuf = wsjtSerial.availableForWrite();

  if(cmd[0] != 0 && sizeBuf > 0) {
    DEBUG_SERIAL2("    T41 sending " + String(cmd))

    // the size of Teensy 4.1 serial transmit buffer is 8k and is used in 4 2k parts.
    // I've seen about 6k available at this point.
    // (https://forum.pjrc.com/index.php?threads/usb-serial-on-teensy-4-0-buffer-size-limitation.67826/)
    int len = strlen(cmd);
    if(wsjtSerial.availableForWrite() > len) {
      //int i=0;
      wsjtSerial.write(cmd, len);
      wsjtSerial.send_now(); // we'll have a delay without this
    } else {
      // send it byte by byte
      int i=0;
      while(cmd[i] != 0) {
        if(wsjtSerial.availableForWrite() > 0) {
          wsjtSerial.print(cmd[i++]);
        } else {
          wsjtSerial.flush(); // *** TODO: this will cause a freeze if PC stops receiving ***
        }
      }
    }
  }
}

void WSJTControlGetCommand(char * cmd, int max) {
  int i = 0;

  while(wsjtSerial.available() > 0) {
    cmd[i] = (char) wsjtSerial.read();

    // there might be multiple commands in the serial buffer
    // read only the first one or up to the specified limit
    if(cmd[i] == ';' || i >= max) {
      break;
    }
    i++;
  }
  cmd[i+1] = 0; // *** TODO: this is currently needed by send command, revisit if that is changed ***
}

// Kenwood Band
int GetKenwoodBand() {
  int band;
  switch(currentBand) {
    case BAND_80M:
      band=1;
      break;
    case BAND_40M:
      band=2;
      break;
    case BAND_20M:
      band=4;
      break;
    case BAND_17M:
      band=5;
      break;
    case BAND_15M:
      band=6;
      break;
    case BAND_12M:
      band=7;
      break;
    case BAND_10M:
      band=8;
      break;
    default:
      band=2; // 40m
      break;
  }
  return band;
}

// Kenwood TS-890S operating modes
int GetKenwoodMode() {
  // 1: LSB, 2: USB, 3: CW, 4: FM, 5: AM
  int mode;
  if(radioMode == CW_MODE) {
    mode=3;
  } else {
    switch(bands[currentBand].demod) {
      case DEMOD_USB:
        mode=2; // USB
        break;
      case DEMOD_LSB:
        mode=1; // LSB
        break;
      case DEMOD_AM:
      case DEMOD_SAM:
        mode=5; // AM
        break;
      case DEMOD_NFM:
        mode=4; // FM
        break;
      default:
        mode=1; // LSB
        break;
    }
  }
  return mode;
}

int GetT41Demod(int mode) {
  // FT8 data mode is always USB
  int demod = DEMOD_USB;
  /*
  int demod = DEMOD_LSB;
  switch(mode) {
    case 1: // LSB
      demod = DEMOD_LSB;
      break;
    case 2: // USB
      demod = DEMOD_USB;
      break;
    case 3: // CW
      demod = CW_MODE;
      break;
    case 4: // FM
      demod = DEMOD_NFM;
      break;
    case 5: // AM
      demod = DEMOD_AM;
      break;
    default:
      demod = DEMOD_LSB;
      break;
  }*/
  return demod;
}

// WSJT-X had trouble with Kenwood TS-2000 use the TS-890S instead
// WSJT-X doesn't model Kenwood TS-890S computer control commands, but
// rather uses a subset of TS-2000 commands.
void WSJTLoop()
{
  if(wsjtSerial.available()) {
    char cmd[256];
    //int mode = GetKenwoodMode();
    int mode = 2; // FT8 mode is always USB

    WSJTControlGetCommand(cmd, 256);
    if(cmd[0] == 0 || cmd[0] == ';') {
      return;
    }

    DEBUG_SERIAL("    T41 received " + String(cmd))

    // *** TODO: some of these need changed from the T41 control app settings ***
    switch(cmd[0]) {
      case 'A':
        if(cmd[1] == 'I' && cmd[2] == ';') {
          sprintf(cmd,"AI0;"); // Auto info off
        } else if(cmd[1] == 'I' && cmd[3] == ';') {
          // auto info command
          return;
        }
        break;

      case 'B':
        if(cmd[1] == 'U' && cmd[2] == ';') {
          // band up
          ChangeBand(1);
          return;
        } else if(cmd[1] == 'D' && cmd[2] == ';') {
          // band up
          ChangeBand(-1);
          return;
        } else if(cmd[1] == 'U' && cmd[3] == ';') {
          if(atoi(&cmd[2]) == 0) {
            sprintf(cmd,"BU0%d;",GetKenwoodBand());
          } else {
            sprintf(cmd,"BU1%d;",GetKenwoodBand());
          }
        } else if(cmd[1] == 'D' && cmd[3] == ';') {
          if(atoi(&cmd[2]) == 0) {
            sprintf(cmd,"BD0%d;",GetKenwoodBand());
          } else {
            sprintf(cmd,"BD1%d;",GetKenwoodBand());
          }
        }
        break;

      case 'F':
        long f;
        switch(cmd[1]) {
          // a frequency change by wsjt is likely to be large and
          // involve a band change, just use center tuning changes
          case 'A':
            if(cmd[13] == ';') {
              // set VFO A frequency
              f = atol(&cmd[2]);
              ChangeBand(f);
              SetCenterTune(f - centerFreq);
              currentFreqA = f;
              return;
            } else if(cmd[2] == ';') {
              // read VFO A frequency
              sprintf(cmd,"FA%011d;",currentFreqA);
            }
            break;

          case 'B':
            if(cmd[13] == ';') {
              // set VFO B frequency
              f = atol(&cmd[2]);
              ChangeBand(f);
              SetCenterTune(f - centerFreq);
              currentFreqB = f;
              return;
            } else if(cmd[2] == ';') {
              // read VFO B frequency
              sprintf(cmd,"FB%011d;",currentFreqB);
            }
            break;

          case 'C':
            if(cmd[13] == ';') {
              // set center frequency
              f = atol(&cmd[2]);
              centerFreq = f;
              NCOFreq = 0L;
              SetTxRxFreq(f);
              DrawBandwidthBar();
              return;
            } else if(cmd[2] == ';') {
              // read center frequency
              sprintf(cmd,"FC%011d;",centerFreq);
            }
            break;

          case 'I':
            if(cmd[4] == ';') {
              // freq or fine tune increment change
              if(cmd[2] == '0') {
                ChangeFreqIncrement(atol(&cmd[3]) - tuneIndex);
              } else if(cmd[2] == '1') {
                ChangeFtIncrement(atol(&cmd[3]) - ftIndex);
              }
            }
            return;
            break;

          case 'R':
            if(cmd[2] == ';') {
              sprintf(cmd,"FR0;"); // receive on VFO A
            } else if(cmd[3] == ';') {
              // select VFO
              VFOSelect(atoi(&cmd[2]));
              return;
            }
            break;

          case 'S':
            if(cmd[3] == ';') {
              // fine tune on or off
              SetFtActive(atoi(&cmd[2]));
              return;
            }
            break;

          case 'T':
            if(cmd[2] == ';') {
              //sprintf(cmd,"FT1;"); // transmit on VFO B
              sprintf(cmd,"FT0;"); // transmit on VFO A
            } else if(cmd[3] == ';') {
              // select VFO
              VFOSelect(atoi(&cmd[2]));
              return;
            }
            break;

          default:
            //cmd[0] = '?';
            //cmd[1] = ';';
            //cmd[2] = 0;
            return;
            break;
        }
        break;

      case 'G':
        if(cmd[1] == 'T' && cmd[3] == ';') {
          // update AGC
          AGCMode = atol(&cmd[2]);
          UpdateInfoBoxItem(IB_ITEM_AGC);
        }
        return;
        break;

      case 'I':
        if(cmd[1] == 'D' && cmd[2] == ';') {
          sprintf(cmd,"ID024;"); // TS-890S
          //sprintf(cmd,"ID019;"); // TS-2000
        } else if(cmd[1] == 'F' && cmd[2] == ';') {
          // retrieves transceiver status
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
          sprintf(cmd, "IF%011d%04d%+06d%d%d%d%02d%d%d%d%d%d%d%02d%d;",
            TxRxFreq,     // freq in Hz
            5000,         // freq step size
            0,            // RIT/XIT freq in Hz, +-99999, this isn't preserved in the T41 but would be VFO A - VFO B if split
            0,            // RIT on/off
            0,            // XIT on/off
            0,0,          // channel bank number
            !GetXRState(),     // RX/TX (1/0)
            mode,         // operating mode
            activeVFO,    // RX VFO
            0,            // scan Status
            0,            // split status (Kenwood manual refers to SP command which doesn't exist)
            0,            // CTCSS enabled
            1,            // CTCSS tone frequency
            0             // shift status
          );
        }
        break;

      case 'K': //
        if(cmd[1] == 'S' && cmd[2] == ';') {
          sprintf(cmd,"KS0%d;", DEFAULT_KEYER_WPM);
        }
        break;

      case 'M': // the TS890S doesn't have this command, but WSJT-X uses it anyway
        if(cmd[1] == 'D' && cmd[2] == ';') {
          // send demod mode
          sprintf(cmd,"MD%d;", mode);
          //sprintf(cmd,"MD%d;", 2);
          //sprintf(cmd,"?;");
        } else if(cmd[1] == 'D' && cmd[3] == ';') {
          // set demod mode status
          // *** FT8 mode is now always USB ***
          //int demod = GetT41Demod(atoi(&cmd[2]));
          //ChangeDemodMode(demod);
          return;
          //sprintf(cmd,"?;");
        } else if(cmd[1] == 'E' && cmd[3] == ';') {
          // set operating mode
          //ChangeMode(atoi(&cmd[2]));
          return;
        }
        break;

      case 'N':
        if(cmd[1] == 'F' && cmd[2] == ';') {
          // send noise floor
          sprintf(cmd,"NF%04d;", currentNoiseFloor[currentBand]);
        } else if(cmd[1] == 'F' && cmd[6] == ';') {
          // set noise floor
          currentNoiseFloor[currentBand] = atoi(&cmd[2]);
          return;
        } else if(cmd[1] == 'G' && cmd[3] == ';') {
          // *** TODO: consider just toggling this through call to
          liveNoiseFloorFlag = atoi(&cmd[2]);

          // save final noise floor setting if toggling flag off
          if(liveNoiseFloorFlag == 0) {
            EEPROMData.currentNoiseFloor[currentBand]  = currentNoiseFloor[currentBand];
            EEPROMWrite();
          }
          UpdateInfoBoxItem(IB_ITEM_FLOOR);
          return;
        }
        break;

      case 'O': // PCxxx;
        if(cmd[1] == 'M' && cmd[3] == ';') {
          // operating demod mode
          int item = cmd[2];
          sprintf(cmd,"OM%d%d;", item, mode);
        } else if(cmd[1] == 'M' && cmd[4] == ';') {
          // set demod mode status
          char val[2] = { cmd[2], 0 };
          //int item = atoi(val);
          val[0] = cmd[3];
          int mode = atoi(val);
          int demod = GetT41Demod(mode);
          if(demod == CW_MODE) {

          } else {
            //ChangeDemodMode(demod);
          }
          return;
        }
        break;

      case 'P': // PCxxx;
        if(cmd[1] == 'C' && cmd[5] == ';') {
          // set transmitter power level
          transmitPowerLevel = atoi(&cmd[2]);
          ShowCurrentPowerSetting();
          return;
        } else if(cmd[1] == 'S' && cmd[2] == ';') {
          //sprintf(cmd,"PS0;"); // 0=Off, 1=On
          sprintf(cmd,"PS1;");
        }
        break;

      case 'R': //
        if(cmd[1] == 'X' && cmd[2] == ';') {
          ft8PTT = false;
          return;
        }
        break;

      case 'S':
        if(cmd[1] == 'F' && cmd[3] == ';') {
          int vfo = atoi(&cmd[2]);
          int freq = vfo == 0 ? currentFreqA : currentFreqB;
          sprintf(cmd,"SF%d%011d%d;", vfo, freq, mode);
        } else if(cmd[1] == 'P' && cmd[3] == ';') { // this is split with DX Lab Suite, but Split Operation Frequency Setting with TS-890S; the reponse works for both with no split operation
          // set split VFO on/off
          return;
        } else if(cmd[1] == 'P' && cmd[2] == ';') {
          //sprintf(cmd,"SP%d;", splitVFO ? 1 : 0);
          sprintf(cmd,"SP%d;", 0); // no split operation
        }
        break;

      case 'T': // split
        if(cmd[1] == 'B' && cmd[2] == ';') {
          //sprintf(cmd,"TB%d;", splitVFO ? 1 : 0);
          sprintf(cmd,"TB%d;", 0);
        } else if(cmd[1] == 'M' && cmd[13] == ';') {
          // set Teensy RTC
          Teensy3Clock.set(atol(&cmd[2]));
          setTime(atol(&cmd[2]));
          return;
        } else if(cmd[1] == 'X' && cmd[2] == ';') {
          ft8PTT = true;
          return;
        }
        break;

      default:
        //cmd[0] = '?';
        //cmd[1] = ';';
        //cmd[2] = 0;
        return;
        break;
    }

    WSJTControlSendCmd(cmd);
  }
}
