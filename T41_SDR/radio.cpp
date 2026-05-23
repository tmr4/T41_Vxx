
#include <TimeLib.h>                   // Part of Teensy Time library

#include "SDT.h"

#include "ButtonProc.h"
#include "Display.h"
#include "Encoders.h"
#include "Filter.h"
#include "hardware.h"
#include "MenuProc.h"
#include "Tune.h"
#include "Utility.h"

#include "radio.h"
#include "connectManager.h"

//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

//extern bool controlDataFlag; // *** data transfers to PC control app are broken ***
extern bool ft8PTT;
extern ConnectManager transport;

//-------------------------------------------------------------------------------------------------------------
// Code
//-------------------------------------------------------------------------------------------------------------

//-------------------------------------------------------------------------------------------------------------
// Generic CAT commands - PC or remote unit control commands
//-------------------------------------------------------------------------------------------------------------

// band down
// "BD;" (length 3) or "BDx;" (length 4)
void RemoteRadio::cat_BD(const char* cmd, bool isRead) {
  if(isRead) {
    ChangeBand(-1);
    // SendAS(); // PC control specific ???
  } else {
    ChangeBand(t41.ActiveBand - atoi(&cmd[2]), false);
  }
}

// band up
// "BU;" (length 3) or "BUx;" (length 4)
void RemoteRadio::cat_BU(const char* cmd, bool isRead) {
  if(isRead) {
    ChangeBand(1);
    // SendAS(); // PC control specific ???
  } else {
    ChangeBand(t41.ActiveBand - atoi(&cmd[2]), false);
  }
}

// set data start
void RemoteRadio::cat_DS(const char* cmd, bool isRead) {
  if(!isRead) {
    // start sending spectrum data
    //controlDataFlag = true;
  }
}

// set data pause
void RemoteRadio::cat_DP(const char* cmd, bool isRead) {
  if(!isRead) {
    // stop sending spectrum data
    //controlDataFlag = false;
  }
}

// read/set VFO A frequency
// "FA;" (length 3) or "FAxxxxxxxxxxx;" (length 14)
void RemoteRadio::cat_FA(const char* cmd, bool isRead) {
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
void RemoteRadio::cat_FB(const char* cmd, bool isRead) {
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
void RemoteRadio::cat_FC(const char* cmd, bool isRead) {
  if(isRead) {
    snprintf(msg, sizeof(msg), "FC%011d;", (int)t41.CenterFreq);
  } else {
    long f = atol(&cmd[2]);
    t41.CenterFreq.Update(f);
    SetFreq(f);
  }
}

// read/set NCO frequency offset
// "FF;" (length 3) or "FFxxxxxxxxxxx;" (length 14)
void RemoteRadio::cat_FF(const char* cmd, bool isRead) {
  if(isRead) {
    snprintf(msg, sizeof(msg), "FF%011d;", (int)t41.NCOFreq);
  } else {
    t41.NCOFreq.Update(atol(&cmd[2]));
  }
}

// read/set (toggle) fine tune status, on/off
// "FS;" (length 3) of "FSx;" (length 4) x= 1 off, 0 on
void RemoteRadio::cat_FS(const char* cmd, bool isRead) {
  if(isRead) {
    snprintf(msg, sizeof(msg), "FSF%d;", !(int)t41.MouseCenterTuneActive);
  } else {
    t41.MouseCenterTuneActive.Update(!atoi(&cmd[2]));
    HighlightTuneInc();
  }
}

// set VFO A or B (non-standard, this is specific for transmit on Kenwood)
// "FTx;" (length 4) x=0 VFO A; x=1 VFO B
void RemoteRadio::cat_FT(const char* cmd, bool isRead) {
  if(!isRead) {
    VFOSelect(atoi(&cmd[2]));
    // SendAS(); // PC control specific ???
  }
}

// read/set center or fine tune frequency increment change
// "F0;" (length 3) or "F0x;" (length 4) x= index
void RemoteRadio::cat_F0(const char* cmd, bool isRead) {
  if(isRead) {
    snprintf(msg, sizeof(msg), "F0%d;", (int)t41.CenterTuneIndex);
  } else {
    ChangeFreqIncrement(atol(&cmd[3]) - t41.CenterTuneIndex, false);
  }
}

// read/set fine tune frequency increment change
// "F1;" (length 3) or "F1x;" (length 4) x= index
void RemoteRadio::cat_F1(const char* cmd, bool isRead) {
  if(isRead) {
    snprintf(msg, sizeof(msg), "F1%d;", (int)t41.FineTuneIndex);
  } else {
    ChangeFtIncrement(atol(&cmd[3]) - t41.FineTuneIndex, false);
  }
}

// read/set AGC (non-standard Kenwood command)
// "GT;" (length 3) or "GTx;" (length 4) x=0 VFO A; x=1 VFO B
void RemoteRadio::cat_GT(const char* cmd, bool isRead) {
  if(isRead) {
    snprintf(msg, sizeof(msg), "GT%d;", (int)t41.AGCMode);
  } else {
    t41.AGCMode.Update(atoi(&cmd[2]));
    UpdateInfoBoxItem(T41_ITEM_AGC);
  }
}

// read radio ID
// "ID;" (length 3), Answer: "IDxxx;" (length 6)
// *** ackIdReceipt is provided to acknowledge receipt of a properly formated reply ***
void RemoteRadio::cat_ID(const char* cmd, bool isRead) {
  if(isRead) {
    snprintf(msg, sizeof(msg), "ID%03d;", (int)t41.RadioID);
    if(transport.isRemote()) heatbeart = millis(); // note time for heartbeat
  } else {
    ackIdReceipt();
    heatbeart = millis(); // note time for heartbeat
  }
  //heatbeart = millis(); // note time for heartbeat
}

// read transceiver status
void RemoteRadio::cat_IF(const char* cmd, bool isRead) {
  if(isRead) {
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
}

// read/set demod mode (non-standard Kenwood TS-2000 command)
// "MD;" (length 3) or "MDx;" (length 4) x= demodulation mode (see SDT.h)
void RemoteRadio::cat_MD(const char* cmd, bool isRead) {
  if(isRead) {
    snprintf(msg, sizeof(msg), "MD%d;", (int)t41.DemodMode);
  } else {
    ChangeDemodMode(atoi(&cmd[2]), false);
    // SendAS(); // PC control specific ???
  }
}

// read/set operating mode
// "ME;" (length 3) or "MEx;" (length 4) x= operating mode (see SDT.h)
void RemoteRadio::cat_ME(const char* cmd, bool isRead) {
  if(isRead) {
    snprintf(msg, sizeof(msg), "ME%d;", (int)t41.DemodMode);
  } else {
    ChangeMode(atoi(&cmd[2]), -1, false);
    // SendAS(); // PC control specific ???
  }
}

// *** TODO: some of these 'N' commands conflict with Kenwood commands
// read/set noise floor
// "NF;" (length 3) or "NFxxxx;" (length 7) xxxx= noise floor
void RemoteRadio::cat_NF(const char* cmd, bool isRead) {
  if(isRead) {
    snprintf(msg, sizeof(msg), "NF%04d;", (int)t41.NoiseFloor);
  } else {
    t41.NoiseFloor.Update(atoi(&cmd[2]));
  }
}

// read/set live noise floor
// "NG;" (length 3) or "NGx;" (length 4) x= 0 off, 1 on
void RemoteRadio::cat_NG(const char* cmd, bool isRead) {
  if(isRead) {
    snprintf(msg, sizeof(msg), "NG%d;", (int)t41.LiveNoiseFloor);
  } else {
    t41.LiveNoiseFloor.Update(atoi(&cmd[2]));
    UpdateInfoBoxItem(T41_ITEM_FLOOR);
  }
}

// read/set high audio filter frequency
// "NH;" (length 3) or "NHxxxxxxxxxxx;" (length 14)
void RemoteRadio::cat_NH(const char* cmd, bool isRead) {
  if(isRead) {
    snprintf(msg, sizeof(msg), "NH%011d;", (int)t41.FilterHiCut);
  } else {
    t41.FilterHiCut.Update(atol(&cmd[2]));

    CalcAudioFilters();
  }
}

// read/set low audio filter frequency
// "NL;" (length 3) or "NLxxxxxxxxxxx;" (length 14)
void RemoteRadio::cat_NL(const char* cmd, bool isRead) {
  if(isRead) {
    snprintf(msg, sizeof(msg), "NL%011d;", (int)t41.FilterLoCut);
  } else {
    t41.FilterLoCut.Update(atol(&cmd[2]));

    CalcAudioFilters();
  }
}

// inc/dec audio filter
void RemoteRadio::cat_NS(const char* cmd, bool isRead) {
  if(!isRead) {
    posFilterEncoder += atoi(&cmd[2]);
    ProcessFilterEncoder();

    CalcAudioFilters();
    UpdateDisplayFilters();
  }
}

// set 0.5kHz-1.5kHz audio filter
void RemoteRadio::cat_NW(const char* cmd, bool isRead) {
  if(!isRead) {
    t41.FilterLoCut.Update(500);
    t41.FilterHiCut.Update(1500);

    CalcAudioFilters();
  }
}

// set noise filter
// "N1;" (length 3) or "N1x;" (length 4) x= NR_OPTIONS
void RemoteRadio::cat_N1(const char* cmd, bool isRead) {
  if(isRead) {
    snprintf(msg, sizeof(msg), "N1%d;", (int)t41.NoiseFilter);
  } else {
    t41.NoiseFilter.Update(atoi(&cmd[2]));
    UpdateInfoBoxItem(T41_ITEM_FILTER);
  }
}

// read/set transmit power level (non-standard Kenwood command)
// "PC;" (length 3) or "PCxx;" (length 5) xx= transmit power level
void RemoteRadio::cat_PC(const char* cmd, bool isRead) {
  if(isRead) {
    snprintf(msg, sizeof(msg), "PC%02d;", (int)t41.TxPower);
  } else {
    t41.TxPower.Update(atoi(&cmd[2]));
    ShowCurrentPowerSetting();
  }
}

// read/set RF gain
// "PG;" (length 3) or "PGxxx;" (length 6) xxx= RF gain in db -60 to 10
void RemoteRadio::cat_PG(const char* cmd, bool isRead) {
  if(isRead) {
    snprintf(msg, sizeof(msg), "PG%+03d;", (int)t41.RFGain);
  } else {
    t41.RFGain.Update(atoi(&cmd[2]));
  }
}

// read S-meter (non-standard Kenwood command)
// "SM;" (length 3)
// "SMx;" (length 4) x= 0: dbm; 1: S-meter
// "SMxyyyyy;" (length 9) x= see above; y= value
void RemoteRadio::cat_SM(const char* cmd, bool isRead) {
  float32_t dbm = CalcSignalStrength();

  if(isRead) {
    // One of the following:
    // send dBm
    //sprintf(cmd, "SM0%+05d;", (int)(dbm * 10));

    // send s-meter
    //sprintf(cmd, "SM20%04d;", smeterPad);

    // just send dBm for now
    snprintf(msg, sizeof(msg), "SM0%+05d;", (int)(dbm * 10));
  // *** TODO: need to resolve this one outlier ***
  //} else {
  //  int index = atoi(&cmd[2]);
  //
  //  // just send dBm for now
  //  snprintf(msg, sizeof(msg), "SM%d%+05d;", index, (int)(dbm * 10));
  } else {
    // One of the following:
    // SM0-xxxx; (receive dBm)
    //sprintf(cmd, "SM0%+05d;", (int)(dbm * 10));

    // send s-meter
    //sprintf(cmd, "SM20%04d;", smeterPad);

    //Serial.print("Received signal strength: ");
    //signalStrengthReceivedIndex = atoi(&cmd[2]);
    //signalStrength = ((float)atoi(&cmd[3])) / 10.0;
    //signalStrengthReceived = true;
    //Serial.println(signalStrength);
  }
}

// set Teensy RTC
// "TMxxxxxxxxxxx;" (length 14)
void RemoteRadio::cat_TM(const char* cmd, bool isRead) {
  if(!isRead) {
    Teensy3Clock.set(atol(&cmd[2]));
    setTime(atol(&cmd[2]));
  }
}

// read/set volume
// "VO;" (length 3) or "VOxxx;" (length 6) xxx= volume 0-100
void RemoteRadio::cat_VO(const char* cmd, bool isRead) {
  if(isRead) {
    snprintf(msg, sizeof(msg), "VO%03d;", (int)t41.AudioVolume);
  } else {
    t41.AudioVolume.Update(atoi(&cmd[2]));
  }
}

// read/set spectrum zoom
// "ZM;" (length 3) or "ZMx;" (length 4) x= zoom (0 to MAX_ZOOM_ENTRIES - 1)
void RemoteRadio::cat_ZM(const char* cmd, bool isRead) {
  if(isRead) {
    snprintf(msg, sizeof(msg), "ZM%d;", (int)t41.SpectrumZoom);
  } else {
    t41.SpectrumZoom.Update(atoi(&cmd[2]));
  }
}

const CATCommand RemoteRadio::catCommands[] = {
  {"BD"_cat, 3,  4, RemoteRadio::cat_BD_Wrapper},   // band down
  {"BU"_cat, 3,  4, RemoteRadio::cat_BU_Wrapper},   // band up
  {"DP"_cat, 0,  3, RemoteRadio::cat_DP_Wrapper},   // pause data transfer
  {"DS"_cat, 0,  3, RemoteRadio::cat_DS_Wrapper},   // start data transfer
  {"FA"_cat, 3, 14, RemoteRadio::cat_FA_Wrapper},   // read/set VFO A frequency
  {"FB"_cat, 3, 14, RemoteRadio::cat_FB_Wrapper},   // read/set VFO B frequency
  {"FC"_cat, 3, 14, RemoteRadio::cat_FC_Wrapper},   // read/set current VFO center frequency
  {"FF"_cat, 3, 14, RemoteRadio::cat_FF_Wrapper},   // read/set NCO frequency offset
  {"FS"_cat, 3,  3, RemoteRadio::cat_FS_Wrapper},   // toggle fine tune status
  {"FT"_cat, 0,  4, RemoteRadio::cat_FT_Wrapper},   // set VFO A or B
  {"F0"_cat, 3,  4, RemoteRadio::cat_F0_Wrapper},   // set center or fine tune increment change
  {"F1"_cat, 3,  4, RemoteRadio::cat_F1_Wrapper},   // set center or fine tune increment change
  {"GT"_cat, 3,  4, RemoteRadio::cat_GT_Wrapper},   // read/set AGC
  {"ID"_cat, 3,  6, RemoteRadio::cat_ID_Wrapper},   // read radio ID
  {"IF"_cat, 3,  0, RemoteRadio::cat_IF_Wrapper},   // read transceiver status
  {"MD"_cat, 3,  4, RemoteRadio::cat_MD_Wrapper},   // read/set demod mode
  {"ME"_cat, 3,  4, RemoteRadio::cat_ME_Wrapper},   // read/set operating mode
  {"NF"_cat, 3,  7, RemoteRadio::cat_NF_Wrapper},   // read/set noise floor
  {"NG"_cat, 3,  4, RemoteRadio::cat_NG_Wrapper},   // set live noise floor
  {"NH"_cat, 3, 14, RemoteRadio::cat_NH_Wrapper},   // set high audio filter frequency
  {"NL"_cat, 3, 14, RemoteRadio::cat_NL_Wrapper},   // set low audio filter frequency
  {"NS"_cat, 0,  5, RemoteRadio::cat_NS_Wrapper},   // inc/dec audio filter
  {"NW"_cat, 0,  3, RemoteRadio::cat_NW_Wrapper},   // set 0.5kHz-1.5kHz audio filter
  {"N1"_cat, 3,  4, RemoteRadio::cat_N1_Wrapper},   // set noise filter
  {"PC"_cat, 3,  5, RemoteRadio::cat_PC_Wrapper},   // read/set transmit power level
  {"PG"_cat, 3,  6, RemoteRadio::cat_PG_Wrapper},   // read/set RF gain
  {"SM"_cat, 3,  4, RemoteRadio::cat_SM_Wrapper},   // read S-meter
  {"TM"_cat, 0, 14, RemoteRadio::cat_TM_Wrapper},   // set Teensy RTC
  {"VO"_cat, 3,  6, RemoteRadio::cat_VO_Wrapper},   // read/set volume
  {"ZM"_cat, 3,  4, RemoteRadio::cat_ZM_Wrapper},   // read/set spectrum zoom
};

//-------------------------------------------------------------------------------------------------------------
// WSJT-X specific commands
//-------------------------------------------------------------------------------------------------------------

// WSJT-X had trouble with Kenwood TS-2000 use the TS-890S instead
// WSJT-X doesn't model Kenwood TS-890S computer control commands, but
// rather uses a subset of TS-2000 commands.

// The methods impliments CAT control for WSJT-X.  It models the very limited subset of CAT commands used by WSJT-X for the Kenwood TS-890-S.
// Note that these reflect the WSJT-X implimentation, not Kenwood's.  It's actually closer to the Kenwood TS-2000.
// These aren't appropriate if you need true TS-890S CAT control.
//
// These can also be used with the T41Sever app with the DX Lab Suite Commander selected as the rig.
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
   The following methods impliments the following CAT commands:
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

    ID - Transceiver ID Number (RA)
      ID;
      ID024; T41 always responds with TS-890S id

      Notes: On receipt of the ID; command the T41 switches to DATA_MODE and DEMOD_FT8 demodulation

    IF - read transceiver status (RA)
      IF;

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

// Auto Information
// "AI;" (length 3) or "AIx;" (length 4) x=0 off; x=1 on?
void RemoteRadio::wsjt_AI(const char* cmd, bool isRead) {
  if(isRead) {
    snprintf(msg, sizeof(msg), "AI0;"); // Auto info off
  } else {
  }
}

// read/set VFO A frequency
// "FA;" (length 3) or "FAxxxxxxxxxxx;" (length 14)
void RemoteRadio::wsjt_FA(const char* cmd, bool isRead) {
  if(isRead) {
    snprintf(msg, sizeof(msg), "FA%011d;", (int)t41.GetFreqA());
  } else {
    long f = atol(&cmd[2]);
    ChangeBand(f);
    t41.SetFreqA(f);
  }
}

// read/set VFO B frequency
// "FB;" (length 3) or "FBxxxxxxxxxxx;" (length 14)
void RemoteRadio::wsjt_FB(const char* cmd, bool isRead) {
  if(isRead) {
    snprintf(msg, sizeof(msg), "FB%011d;", (int)t41.GetFreqB());
  } else {
    long f = atol(&cmd[2]);
    ChangeBand(f);
    t41.SetFreqB(f);
  }
}

// read/set VFO A or B
// "FTx;" (length 4) x=0 VFO A; x=1 VFO B
void RemoteRadio::wsjt_FT(const char* cmd, bool isRead) {
  if(isRead) {
    snprintf(msg, sizeof(msg), "FT0;"); // T41 always responds transmit on VFO A
  } else {
    VFOSelect(atoi(&cmd[2]));
  }
}

// read radio ID
// "ID;" (length 3), Answer: "IDxxx;" (length 6)
// Kenwood TS-890S: ID024; // *** WSJT-X expects this even when TS-2000 is selected ***
// Kenwood TS-2000: ID019;
void RemoteRadio::wsjt_ID(const char* cmd, bool isRead) {
  if(isRead) {
    // receipt of ID command will switch to FT8 Data mode if not already there
    ChangeMode(DATA_MODE, DEMOD_FT8);

    // reply with the TS-890S id
    snprintf(msg, sizeof(msg), "ID024;");
    //snprintf(msg, sizeof(msg), "ID019;"); // TS-2000
  }
}

// read transceiver status
// "IF;" (length = 3)
void RemoteRadio::wsjt_IF(const char* cmd, bool isRead) {
  if(isRead) {
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
      mode,         // operating mode
      (int)t41.ActiveVFO,    // RX VFO
      0,            // scan Status
      0,            // split status (Kenwood manual refers to SP command which doesn't exist)
      0,            // CTCSS enabled
      1,            // CTCSS tone frequency
      0             // shift status
    );
  }
}

// read keying Speed
// "KS;" (length 3)
void RemoteRadio::wsjt_KS(const char* cmd, bool isRead) {
  if(isRead) {
    snprintf(msg, sizeof(msg), "KS0%d;", DEFAULT_KEYER_WPM);
  }
}

// read demod mode
// "MD;" (length 3)
void RemoteRadio::wsjt_MD(const char* cmd, bool isRead) {
  if(isRead) {
    snprintf(msg, sizeof(msg), "MD%d;", mode);
  }
}

// read VFO frequency and mode
// "SF;" (length 3)
void RemoteRadio::wsjt_SF(const char* cmd, bool isRead) {
  if(isRead) {
    int vfo = atoi(&cmd[2]);
    int freq = vfo == 0 ? t41.GetFreqA() : t41.GetFreqB();
    snprintf(msg, sizeof(msg), "SF%d%011d%d;", vfo, freq, mode);
  }
}

// Split VFO
// "SP;" (length 3) or "SPx;" (length 4)
void RemoteRadio::wsjt_SP(const char* cmd, bool isRead) {
  if(isRead) {
    snprintf(msg, sizeof(msg), "SP%d;", 0); // no split VFO
  } else {
    // ignored
  }
}

// Split
// "TB;" (length 3) or "TBx;" (length 4)
void RemoteRadio::wsjt_TB(const char* cmd, bool isRead) {
  if(isRead) {
    snprintf(msg, sizeof(msg), "TB%d;", 0); // no split VFO
  } else {
    // ignored
  }
}

// set TX
// "TX;" (length 3)
void RemoteRadio::wsjt_TX(const char* cmd, bool isRead) {
  if(!isRead) {
    ft8PTT = true;
  }
}

const CATCommand RemoteRadio::wsjtCommands[] = {
  {"AI"_cat, 3,  0, RemoteRadio::wsjt_AI_Wrapper},   // auto information
  {"FA"_cat, 3, 14, RemoteRadio::wsjt_FA_Wrapper},   // read/set VFO A frequency
  {"FB"_cat, 3, 14, RemoteRadio::wsjt_FB_Wrapper},   // read/set VFO B frequency
  {"FT"_cat, 3,  4, RemoteRadio::wsjt_FT_Wrapper},   // set VFO A or B
  {"ID"_cat, 3,  0, RemoteRadio::wsjt_ID_Wrapper},   // read radio ID
  {"IF"_cat, 3,  0, RemoteRadio::wsjt_IF_Wrapper},   // read transceiver status
  {"KS"_cat, 3,  0, RemoteRadio::wsjt_KS_Wrapper},   // key speed
  {"MD"_cat, 3,  0, RemoteRadio::wsjt_MD_Wrapper},   // read/set demod mode
  {"SF"_cat, 3,  0, RemoteRadio::wsjt_SF_Wrapper},   // read VFO freq and mode
  {"SP"_cat, 3,  4, RemoteRadio::wsjt_SP_Wrapper},   // read split VFO
  {"TB"_cat, 3,  4, RemoteRadio::wsjt_TB_Wrapper},   // read split
  {"TM"_cat, 0, 14, RemoteRadio::cat_TM_Wrapper},    // set Teensy RTC (allows PC app to set radio time)
  {"TX"_cat, 0,  3, RemoteRadio::wsjt_TX_Wrapper},   // TX
};
