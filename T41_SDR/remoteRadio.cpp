
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

#include "remoteRadio.h"

//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

//extern bool controlDataFlag; // *** data transfers to PC control app are broken ***
extern bool ft8PTT;

//-------------------------------------------------------------------------------------------------------------
// Code
//-------------------------------------------------------------------------------------------------------------

//-------------------------------------------------------------------------------------------------------------
// Generic CAT commands - PC or remote unit control commands
//-------------------------------------------------------------------------------------------------------------

// *** comments below show both read/set command structure,
//     but these functions only cover set commands ***

// read/set band down
// "BD;" (length 3) or "BDx;" (length 4)
void RemoteRadio::cat_BD(const char* cmd) {
  ChangeBand(t41.ActiveBand - atoi(&cmd[2]), false);
}

// read/set band up
// "BU;" (length 3) or "BUx;" (length 4)
void RemoteRadio::cat_BU(const char* cmd) {
  ChangeBand(t41.ActiveBand - atoi(&cmd[2]), false);
}

// set data start
void RemoteRadio::cat_DS(const char* cmd) {
  // start sending spectrum data
  //controlDataFlag = true;
}

// set data pause
void RemoteRadio::cat_DP(const char* cmd) {
  // stop sending spectrum data
  //controlDataFlag = false;
}

// read/set VFO A frequency
// "FA;" (length 3) or "FAxxxxxxxxxxx;" (length 14)
void RemoteRadio::cat_FA(const char* cmd) {
  long f = atol(&cmd[2]);

  ChangeBand(f);
  if(t41.MouseCenterTuneActive) {
    t41.SetFreqA(f);
  } else {
    t41.NCOFreq.Update(f); // *** TODO: verify, this should only happen on active VFO ***
  }
}

// read/set VFO B frequency
// "FB;" (length 3) or "FBxxxxxxxxxxx;" (length 14)
void RemoteRadio::cat_FB(const char* cmd) {
  long f = atol(&cmd[2]);

  ChangeBand(f);
  if(t41.MouseCenterTuneActive) {
    t41.SetFreqB(f);
  } else {
    t41.NCOFreq.Update(f); // *** TODO: verify, this should only happen on active VFO ***
  }
}

// read/set current VFO center frequency
// "FC;" (length 3) or "FCxxxxxxxxxxx;" (length 14)
void RemoteRadio::cat_FC(const char* cmd) {
  long f = atol(&cmd[2]);
  t41.CenterFreq.Update(f);
  SetFreq(f);
}

// read/set NCO frequency offset
// "FF;" (length 3) or "FFxxxxxxxxxxx;" (length 14)
void RemoteRadio::cat_FF(const char* cmd) {
  t41.NCOFreq.Update(atol(&cmd[2]));
}

// read/set (toggle) fine tune status, on/off
// "FS;" (length 3) of "FSx;" (length 4) x= 1 off, 0 on
void RemoteRadio::cat_FS(const char* cmd) {
  t41.MouseCenterTuneActive.Update(!atoi(&cmd[2]));
  HighlightTuneInc();
}

// set VFO A or B (non-standard, this is specific for transmit on Kenwood)
// "FTx;" (length 4) x=0 VFO A; x=1 VFO B
void RemoteRadio::cat_FT(const char* cmd) {
  VFOSelect(atoi(&cmd[2]));
  // SendAS(); // PC control specific ???
}

// read/set center or fine tune frequency increment change
// "F0;" (length 3) or "F0x;" (length 4) x= index
void RemoteRadio::cat_F0(const char* cmd) {
  ChangeFreqIncrement(atol(&cmd[3]) - t41.CenterTuneIndex, false);
}

// read/set fine tune frequency increment change
// "F1;" (length 3) or "F1x;" (length 4) x= index
void RemoteRadio::cat_F1(const char* cmd) {
  ChangeFtIncrement(atol(&cmd[3]) - t41.FineTuneIndex, false);
}

// read/set AGC (non-standard Kenwood command)
// "GT;" (length 3) or "GTx;" (length 4) x=0 VFO A; x=1 VFO B
void RemoteRadio::cat_GT(const char* cmd) {
  t41.AGCMode.Update(atoi(&cmd[2]));
  UpdateInfoBoxItem(T41_ITEM_AGC);
}

// read radio ID
// "ID;" (length 3), Answer: "IDxxx;" (length 6)
// *** ackIdReceipt is provided to acknowledge receipt of a properly formated reply ***
void RemoteRadio::cat_ID(const char* cmd) {
  ackIdReceipt();
  heartbeat = millis(); // note time for heartbeat
}

// read transceiver status
void RemoteRadio::cat_IF(const char* cmd) {}

// read/set demod mode (non-standard Kenwood TS-2000 command)
// "MD;" (length 3) or "MDx;" (length 4) x= demodulation mode (see SDT.h)
void RemoteRadio::cat_MD(const char* cmd) {
  ChangeDemodMode(atoi(&cmd[2]), false);
  // SendAS(); // PC control specific ???
}

// read/set operating mode
// "ME;" (length 3) or "MEx;" (length 4) x= operating mode (see SDT.h)
void RemoteRadio::cat_ME(const char* cmd) {
  ChangeMode(atoi(&cmd[2]), -1, false);
  // SendAS(); // PC control specific ???
}

// *** TODO: some of these 'N' commands conflict with Kenwood commands
// read/set noise floor
// "NF;" (length 3) or "NFxxxx;" (length 7) xxxx= noise floor
void RemoteRadio::cat_NF(const char* cmd) {
  t41.NoiseFloor.Update(atoi(&cmd[2]));
}

// read/set live noise floor
// "NG;" (length 3) or "NGx;" (length 4) x= 0 off, 1 on
void RemoteRadio::cat_NG(const char* cmd) {
  t41.LiveNoiseFloor.Update(atoi(&cmd[2]));
  UpdateInfoBoxItem(T41_ITEM_FLOOR);
}

// read/set high audio filter frequency
// "NH;" (length 3) or "NHxxxxxxxxxxx;" (length 14)
void RemoteRadio::cat_NH(const char* cmd) {
  t41.FilterHiCut.Update(atol(&cmd[2]));
  CalcAudioFilters();
}

// read/set low audio filter frequency
// "NL;" (length 3) or "NLxxxxxxxxxxx;" (length 14)
void RemoteRadio::cat_NL(const char* cmd) {
  t41.FilterLoCut.Update(atol(&cmd[2]));
  CalcAudioFilters();
}

// inc/dec audio filter
void RemoteRadio::cat_NS(const char* cmd) {
  posFilterEncoder += atoi(&cmd[2]);
  ProcessFilterEncoder();

  CalcAudioFilters();
  UpdateDisplayFilters();
}

// set 0.5kHz-1.5kHz audio filter
void RemoteRadio::cat_NW(const char* cmd) {
  t41.FilterLoCut.Update(500);
  t41.FilterHiCut.Update(1500);

  CalcAudioFilters();
}

// set noise filter
// "N1;" (length 3) or "N1x;" (length 4) x= NR_OPTIONS
void RemoteRadio::cat_N1(const char* cmd) {
  t41.NoiseFilter.Update(atoi(&cmd[2]));
  UpdateInfoBoxItem(T41_ITEM_FILTER);
}

// read/set transmit power level (non-standard Kenwood command)
// "PC;" (length 3) or "PCxx;" (length 5) xx= transmit power level
void RemoteRadio::cat_PC(const char* cmd) {
  t41.TxPower.Update(atoi(&cmd[2]));
  ShowCurrentPowerSetting();
}

// read/set RF gain
// "PG;" (length 3) or "PGxxx;" (length 6) xxx= RF gain in db -60 to 10
void RemoteRadio::cat_PG(const char* cmd) {
  t41.RFGain.Update(atoi(&cmd[2]));
}

// read S-meter (non-standard Kenwood command)
// "SM;" (length 3)
// "SMx;" (length 4) x= 0: dbm; 1: S-meter
// "SMxyyyyy;" (length 9) x= see above; y= value
void RemoteRadio::cat_SM(const char* cmd) {}

// set Teensy RTC
// "TMxxxxxxxxxxx;" (length 14)
void RemoteRadio::cat_TM(const char* cmd) {
  Teensy3Clock.set(atol(&cmd[2]));
  setTime(atol(&cmd[2]));
}

// read/set volume
// "VO;" (length 3) or "VOxxx;" (length 6) xxx= volume 0-100
void RemoteRadio::cat_VO(const char* cmd) {
  t41.AudioVolume.Update(atoi(&cmd[2]));
}

// read/set spectrum zoom
// "ZM;" (length 3) or "ZMx;" (length 4) x= zoom (0 to MAX_ZOOM_ENTRIES - 1)
void RemoteRadio::cat_ZM(const char* cmd) {
  t41.SpectrumZoom.Update(atoi(&cmd[2]));
}

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
void RemoteRadio::wsjt_AI(const char* cmd) {}

// read/set VFO A frequency
// "FA;" (length 3) or "FAxxxxxxxxxxx;" (length 14)
void RemoteRadio::wsjt_FA(const char* cmd) {
  long f = atol(&cmd[2]);
  //ChangeBand(f);
  t41.SetFreqA(f);
}

// read/set VFO B frequency
// "FB;" (length 3) or "FBxxxxxxxxxxx;" (length 14)
void RemoteRadio::wsjt_FB(const char* cmd) {
  long f = atol(&cmd[2]);
  //ChangeBand(f);
  t41.SetFreqB(f);
}

// read/set VFO A or B
// "FTx;" (length 4) x=0 VFO A; x=1 VFO B
void RemoteRadio::wsjt_FT(const char* cmd) {
  VFOSelect(atoi(&cmd[2]));
}

// read keying Speed
// "KS;" (length 3)
void RemoteRadio::wsjt_KS(const char* cmd) {}

// read VFO frequency and mode
// "SF;" (length 3)
void RemoteRadio::wsjt_SF(const char* cmd) {}

// Split VFO
// "SP;" (length 3) or "SPx;" (length 4)
void RemoteRadio::wsjt_SP(const char* cmd) {}

// Split
// "TB;" (length 3) or "TBx;" (length 4)
void RemoteRadio::wsjt_TB(const char* cmd) {}

// set TX
// "TX;" (length 3)
void RemoteRadio::wsjt_TX(const char* cmd) {
  ft8PTT = true;
}

// build the command tables
struct RemoteCommandTable {
  const CATCommand* data[128];

  constexpr RemoteCommandTable() : data{} {
    data["BD"_cath] = &RemoteRadio::cat_BD_cmd;
    data["BU"_cath] = &RemoteRadio::cat_BU_cmd;
    data["DP"_cath] = &RemoteRadio::cat_DP_cmd;
    data["DS"_cath] = &RemoteRadio::cat_DS_cmd;
    data["FA"_cath] = &RemoteRadio::cat_FA_cmd;
    data["FB"_cath] = &RemoteRadio::cat_FB_cmd;
    data["FC"_cath] = &RemoteRadio::cat_FC_cmd;
    data["FF"_cath] = &RemoteRadio::cat_FF_cmd;
    data["FS"_cath] = &RemoteRadio::cat_FS_cmd;
    data["FT"_cath] = &RemoteRadio::cat_FT_cmd;
    data["F0"_cath] = &RemoteRadio::cat_F0_cmd;
    data["F1"_cath] = &RemoteRadio::cat_F1_cmd;
    data["GT"_cath] = &RemoteRadio::cat_GT_cmd;
    data["ID"_cath] = &RemoteRadio::cat_ID_cmd;
    data["IF"_cath] = &RemoteRadio::cat_IF_cmd;
    data["MD"_cath] = &RemoteRadio::cat_MD_cmd;
    data["ME"_cath] = &RemoteRadio::cat_ME_cmd;
    data["NF"_cath] = &RemoteRadio::cat_NF_cmd;
    data["NG"_cath] = &RemoteRadio::cat_NG_cmd;
    data["NH"_cath] = &RemoteRadio::cat_NH_cmd;
    data["NL"_cath] = &RemoteRadio::cat_NL_cmd;
    data["NS"_cath] = &RemoteRadio::cat_NS_cmd;
    data["NW"_cath] = &RemoteRadio::cat_NW_cmd;
    data["N1"_cath] = &RemoteRadio::cat_N1_cmd;
    data["PC"_cath] = &RemoteRadio::cat_PC_cmd;
    data["PG"_cath] = &RemoteRadio::cat_PG_cmd;
    data["SM"_cath] = &RemoteRadio::cat_SM_cmd;
    data["TM"_cath] = &RemoteRadio::cat_TM_cmd;
    data["VO"_cath] = &RemoteRadio::cat_VO_cmd;
    data["ZM"_cath] = &RemoteRadio::cat_ZM_cmd;
  }
};

// *** this table hasn't been tested yet ***
struct WSJTCommandBuilder {
  const CATCommand* data[128];

  constexpr WSJTCommandBuilder() : data{} {
    data["AI"_cath] = &RemoteRadio::wsjt_AI_cmd;
    data["FA"_cath] = &RemoteRadio::wsjt_FA_cmd;
    data["FB"_cath] = &RemoteRadio::wsjt_FB_cmd;
    data["FT"_cath] = &RemoteRadio::wsjt_FT_cmd;
    data["ID"_cath] = &RemoteRadio::cat_ID_cmd;
    data["IF"_cath] = &RemoteRadio::cat_IF_cmd;
    data["KS"_cath] = &RemoteRadio::wsjt_KS_cmd;
    data["MD"_cath] = &RemoteRadio::cat_MD_cmd;
    data["SF"_cath] = &RemoteRadio::wsjt_SF_cmd;
    data["SP"_cath] = &RemoteRadio::wsjt_SP_cmd;
    data["TB"_cath] = &RemoteRadio::wsjt_TB_cmd;
    data["TM"_cath] = &RemoteRadio::cat_TM_cmd;
    data["TX"_cath] = &RemoteRadio::wsjt_TX_cmd;
  }
};

// *** this is declared here after the CAT command tables have been constructed ***

// *** use of __attribute__((section(".progmem.data") vs PROGMEM resolves MethodName##_Wrapper
//     section type conflict with catCommands set as PROGMEM or can use PROGMEM here and use
//     __attribute__((section(".progmem.data") on catCommands ***
static const RemoteCommandTable catCommands __attribute__((section(".progmem.data")));
static const WSJTCommandBuilder wsjtCommands __attribute__((section(".progmem.data")));

#if T41_WSJT_CAT_AUDIO
RemoteRadio remoteRadio(&catCommands.data[0], &wsjtCommands.data[0]);
#else
RemoteRadio remoteRadio(&catCommands.data[0]);
#endif
