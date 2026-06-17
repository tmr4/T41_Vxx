
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

/*

This file defines the available CAT set functions, command tables and CatControl
objects for the selected radio role.  The set functions are defined in the form:

   void supportedCommand(CatControl* instance, const char* cmd) {}

A helping macro simplifies preparing the command table. It's of the form:

  DEFINE_CAT_CMD_PROP(supportedCommand, token, answerFormatStr, readLen, setLen);

A CAT command table can be use alone or passed a connection manager.
See the bottom of file for an example command table.

*/

//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

//-------------------------------------------------------------------------------------------------------------
// Code
//-------------------------------------------------------------------------------------------------------------

//-------------------------------------------------------------------------------------------------------------
// Generic CAT commands - PC or remote unit control commands
//-------------------------------------------------------------------------------------------------------------

// *********************************************************
//     comments below show both read/set command structure,
//     but these function bodies only cover set commands
// *********************************************************

// read/set band down
// "BD;" (length 3) or "BDx;" (length 4)
void cat_BD(CatControl* instance, const char* cmd) {
  //ChangeBand(t41.ActiveBand - atoi(&cmd[2]), false);
  ChangeBand(atoi(&cmd[2]), false);
}

// read/set band up
// "BU;" (length 3) or "BUx;" (length 4)
void cat_BU(CatControl* instance, const char* cmd) {
  //ChangeBand(t41.ActiveBand - atoi(&cmd[2]), false);
  ChangeBand(atoi(&cmd[2]), false);
}

// set data start
void cat_DS(CatControl* instance, const char* cmd) {
  // start sending spectrum data
  //controlDataFlag = true;
}

// set data pause
void cat_DP(CatControl* instance, const char* cmd) {
  // stop sending spectrum data
  //controlDataFlag = false;
}

// read/set VFO A frequency
// "FA;" (length 3) or "FAxxxxxxxxxxx;" (length 14)
void cat_FA(CatControl* instance, const char* cmd) {
  long f = atol(&cmd[2]);

  ChangeBand(f);
  if(instance->isWSJT() || t41.MouseCenterTuneActive) {
    t41.SetFreqA(f);
  } else {
    t41.NCOFreq.Update(f); // *** TODO: verify, this should only happen on active VFO ***
  }
}

// read/set VFO B frequency
// "FB;" (length 3) or "FBxxxxxxxxxxx;" (length 14)
void cat_FB(CatControl* instance, const char* cmd) {
  long f = atol(&cmd[2]);

  ChangeBand(f);
  if(instance->isWSJT() || t41.MouseCenterTuneActive) {
    t41.SetFreqB(f);
  } else {
    t41.NCOFreq.Update(f); // *** TODO: verify, this should only happen on active VFO ***
  }
}

// read/set current VFO center frequency
// "FC;" (length 3) or "FCxxxxxxxxxxx;" (length 14)
void cat_FC(CatControl* instance, const char* cmd) {
  long f = atol(&cmd[2]);
  t41.CenterFreq.Update(f);
  SetFreq(f);
}

// read/set NCO frequency offset
// "FF;" (length 3) or "FFxxxxxxxxxxx;" (length 14)
void cat_FF(CatControl* instance, const char* cmd) {
  t41.NCOFreq.Update(atol(&cmd[2]));
}

// read/set (toggle) fine tune status, on/off
// "FS;" (length 3) of "FSx;" (length 4) x= 1 off, 0 on
void cat_FS(CatControl* instance, const char* cmd) {
  t41.MouseCenterTuneActive.Update(!atoi(&cmd[2]));
  HighlightTuneInc();
}

// set VFO A or B (non-standard, this is specific for transmit on Kenwood)
// "FTx;" (length 4) x=0 VFO A; x=1 VFO B
void cat_FT(CatControl* instance, const char* cmd) {
  VFOSelect(atoi(&cmd[2]));
  // SendAS(); // PC control specific ???
}

// read/set center or fine tune frequency increment change
// "F0;" (length 3) or "F0x;" (length 4) x= index
void cat_F0(CatControl* instance, const char* cmd) {
  ChangeFreqIncrement(atol(&cmd[3]) - t41.CenterTuneIndex, false);
}

// read/set fine tune frequency increment change
// "F1;" (length 3) or "F1x;" (length 4) x= index
void cat_F1(CatControl* instance, const char* cmd) {
  ChangeFtIncrement(atol(&cmd[3]) - t41.FineTuneIndex, false);
}

// read/set AGC (non-standard Kenwood command)
// "GT;" (length 3) or "GTx;" (length 4) x=0 VFO A; x=1 VFO B
void cat_GT(CatControl* instance, const char* cmd) {
  t41.AGCMode.Update(atoi(&cmd[2]));
  UpdateInfoBoxItem(T41_ITEM_AGC);
}

// normal CAT usage:
// read radio ID
// "ID;" (length 3), Answer: "IDxxx;" (length 6)
// In the T41 it's used as a heartbeat indicator from remote units
// *** ackIdReceipt is provided to acknowledge receipt of a properly formated reply ***
// *** this is treated as a set command even though ID isn't changed ***
void cat_ID(CatControl* instance, const char* cmd) {
  instance->ackIdReceipt();
  instance->setHeartbeat(millis()); // note time for heartbeat
}

// read transceiver status
void cat_IF(CatControl* instance, const char* cmd) {}

// read/set demod mode (non-standard Kenwood TS-2000 command)
// "MD;" (length 3) or "MDx;" (length 4) x= demodulation mode (see SDT.h)
void cat_MD(CatControl* instance, const char* cmd) {
  ChangeDemodMode(atoi(&cmd[2]), false);
  // SendAS(); // PC control specific ???
}

// read/set operating mode
// "ME;" (length 3) or "MEx;" (length 4) x= operating mode (see SDT.h)
void cat_ME(CatControl* instance, const char* cmd) {
  ChangeMode(atoi(&cmd[2]), -1, false);
  // SendAS(); // PC control specific ???
}

// *** TODO: some of these 'N' commands conflict with Kenwood commands
// read/set noise floor
// "NF;" (length 3) or "NFxxxx;" (length 7) xxxx= noise floor
void cat_NF(CatControl* instance, const char* cmd) {
  t41.NoiseFloor.Update(atoi(&cmd[2]));
}

// read/set live noise floor
// "NG;" (length 3) or "NGx;" (length 4) x= 0 off, 1 on
void cat_NG(CatControl* instance, const char* cmd) {
  t41.LiveNoiseFloor.Update(atoi(&cmd[2]));
  UpdateInfoBoxItem(T41_ITEM_FLOOR);
}

// read/set high audio filter frequency
// "NH;" (length 3) or "NHxxxxxxxxxxx;" (length 14)
void cat_NH(CatControl* instance, const char* cmd) {
  t41.FilterHiCut.Update(atol(&cmd[2]));
  CalcAudioFilters();
}

// read/set low audio filter frequency
// "NL;" (length 3) or "NLxxxxxxxxxxx;" (length 14)
void cat_NL(CatControl* instance, const char* cmd) {
  t41.FilterLoCut.Update(atol(&cmd[2]));
  CalcAudioFilters();
}

// inc/dec audio filter
void cat_NS(CatControl* instance, const char* cmd) {
  posFilterEncoder += atoi(&cmd[2]);
  ProcessFilterEncoder();

  CalcAudioFilters();
  UpdateDisplayFilters();
}

// set 0.5kHz-1.5kHz audio filter
void cat_NW(CatControl* instance, const char* cmd) {
  t41.FilterLoCut.Update(500);
  t41.FilterHiCut.Update(1500);

  CalcAudioFilters();
}

// set noise filter
// "N1;" (length 3) or "N1x;" (length 4) x= NR_OPTIONS
void cat_N1(CatControl* instance, const char* cmd) {
  t41.NoiseFilter.Update(atoi(&cmd[2]));
  UpdateInfoBoxItem(T41_ITEM_FILTER);
}

// read/set transmit power level (non-standard Kenwood command)
// "PC;" (length 3) or "PCxx;" (length 5) xx= transmit power level
void cat_PC(CatControl* instance, const char* cmd) {
  t41.TxPower.Update(atoi(&cmd[2]));
  ShowCurrentPowerSetting();
}

// read/set RF gain
// "PG;" (length 3) or "PGxxx;" (length 6) xxx= RF gain in db -60 to 10
void cat_PG(CatControl* instance, const char* cmd) {
  t41.RFGain.Update(atoi(&cmd[2]));
}

// read S-meter (non-standard Kenwood command)
// "SM;" (length 3)
// "SMx;" (length 4) x= 0: dbm; 1: S-meter
// "SMxyyyyy;" (length 9) x= see above; y= value
void cat_SM(CatControl* instance, const char* cmd) {}

// set Teensy RTC
// "TMxxxxxxxxxxx;" (length 14)
void cat_TM(CatControl* instance, const char* cmd) {
  Teensy3Clock.set(atol(&cmd[2]));
  setTime(atol(&cmd[2]));
}

// read/set volume
// "VO;" (length 3) or "VOxxx;" (length 6) xxx= volume 0-100
void cat_VO(CatControl* instance, const char* cmd) {
  t41.AudioVolume.Update(atoi(&cmd[2]));
}

// read/set spectrum zoom
// "ZM;" (length 3) or "ZMx;" (length 4) x= zoom (0 to MAX_ZOOM_ENTRIES - 1)
void cat_ZM(CatControl* instance, const char* cmd) {
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
// WSJT-X starts by issuing the following commands with TS-890S selected as the rig, commands in () may be included:
//  ID; PS; IF; AI; KS; FA; FT; TB; SF0; IF; FA000xxxxx055; FA; FA; FA; FA000xxxxx000; IF; FT; TB; SF0;
// *** sometimes IF; replaces FT; and sometime FB; FT; is inserted ***
// *** sometimes a command may be repeated, especially on communication error ***
// *** WSJT-X seems pretty forgiving as long as ID and FA are as expected ***
//
// WSJT-X does a periodic poll which issues the following commands in order and expects the normal answer:
//  w/ TS-890S rig: FT; TB; FA; SF0; (sometime IF; is used in place of FT;)
//  w/ T41Server: FA; SP; MD;
//
// When band/freq change is selected in WSJT-X: FA; FA000xxxxxxxx; ID; SF0; (unit must set band if needed)
//
// Other commands:
//  TX; set command sent at beginning of transmission interval (usually followed by ID;)
//  RX; set command sent at end of transmission interval
//
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
      T41 always returns USB, no split mode operation
      x: 0=VFO A; 1=VFO B, y:VFO freq, z=2 USB
      SFxyyyyyyyyyyyz;
      SFx;
      SFxyyyyyyyyyyyz;

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

// reset TX
// "RX;" (length 3)
void wsjtCat_RX(CatControl* instance, const char* cmd) {
  t41.wsjtPTT = 0;
}

// set TX
// "TX;" (length 3)
void wsjtCat_TX(CatControl* instance, const char* cmd) {
  t41.wsjtPTT = 1;
}

// command table construction helpers (see catHelper.h)
// generic commands
DEFINE_CAT_CMD_PROP("BD"_cat,  &t41.ActiveBand,             cat_BD, "BD%d;",     3,  4); // band down
DEFINE_CAT_CMD_PROP("BU"_cat,  &t41.ActiveBand,             cat_BU, "BU%d;",     3,  4); // band up
DEFINE_CAT_CMD_ACTN("DP"_cat,  nullptr,                     cat_DP, "",          0,  3); // pause data transfer
DEFINE_CAT_CMD_ACTN("DS"_cat,  nullptr,                     cat_DS, "",          0,  3); // start data transfer
DEFINE_CAT_CMD_ACTN("FA"_cat,  nullptr,                     cat_FA, "FA%011d;",  3, 14); // read/set VFO A frequency
DEFINE_CAT_CMD_ACTN("FB"_cat,  nullptr,                     cat_FB, "FB%011d;",  3, 14); // read/set VFO B frequency
DEFINE_CAT_CMD_PROP("FC"_cat,  &t41.CenterFreq,             cat_FC, "FC%011d;",  3, 14); // read/set current VFO center frequency
DEFINE_CAT_CMD_PROP("FF"_cat,  &t41.NCOFreq,                cat_FF, "FF%011d;",  3, 14); // read/set NCO frequency offset
DEFINE_CAT_CMD_PROP("FS"_cat,  &t41.MouseCenterTuneActive,  cat_FS, "FS%d;",     3,  3); // toggle fine tune status
DEFINE_CAT_CMD_PROP("FT"_cat,  &t41.wsjtFT,                 cat_FT, "FT%d;",     3,  4); // set TX VFO A or B *** just configured for WSJT-X for now as VFO A ***
DEFINE_CAT_CMD_PROP("F0"_cat,  &t41.CenterTuneIndex,        cat_F0, "F0%d;",     3,  4); // set center or fine tune increment change
DEFINE_CAT_CMD_PROP("F1"_cat,  &t41.FineTuneIndex,          cat_F1, "F1%d;",     3,  4); // set center or fine tune increment change
DEFINE_CAT_CMD_PROP("GT"_cat,  &t41.AGCMode,                cat_GT, "GT%d;",     3,  4); // read/set AGC
DEFINE_CAT_CMD_ACTN("ID"_cat,  nullptr,                     cat_ID, "ID%03d;",   3,  6); // read radio ID
DEFINE_CAT_CMD_ACTN("IF"_cat,  nullptr,                     cat_IF, "",          3,  0); // read transceiver status
DEFINE_CAT_CMD_PROP("MD"_cat,  &t41.DemodMode,              cat_MD, "MD%d;",     3,  4); // read/set demod mode
DEFINE_CAT_CMD_PROP("ME"_cat,  &t41.RadioMode,              cat_ME, "ME%d;",     3,  4); // read/set operating mode
DEFINE_CAT_CMD_PROP("NF"_cat,  &t41.NoiseFloor,             cat_NF, "NF%04d;",   3,  7); // read/set noise floor
DEFINE_CAT_CMD_PROP("NG"_cat,  &t41.LiveNoiseFloor,         cat_NG, "NG%d;",     3,  4); // set live noise floor
DEFINE_CAT_CMD_PROP("NH"_cat,  &t41.FilterHiCut,            cat_NH, "NH%011d;",  3, 14); // set high audio filter frequency
DEFINE_CAT_CMD_PROP("NL"_cat,  &t41.FilterLoCut,            cat_NL, "NL%011d;",  3, 14); // set low audio filter frequency
DEFINE_CAT_CMD_ACTN("NS"_cat,  nullptr,                     cat_NS, "",          0,  5); // inc/dec audio filter
DEFINE_CAT_CMD_ACTN("NW"_cat,  nullptr,                     cat_NW, "",          0,  3); // set 0.5kHz-1.5kHz audio filter
DEFINE_CAT_CMD_PROP("N1"_cat,  &t41.NoiseFilter,            cat_N1, "N1%d;",     3,  4); // set noise filter
DEFINE_CAT_CMD_PROP("PC"_cat,  &t41.TxPower,                cat_PC, "PC%02d;",   3,  5); // read/set transmit power level
DEFINE_CAT_CMD_PROP("PG"_cat,  &t41.RFGain,                 cat_PG, "PG%+03d;",  3,  6); // read/set RF gain
DEFINE_CAT_CMD_ACTN("SM"_cat,  nullptr,                     cat_SM, "SM0%+05d;", 3,  4); // read S-meter
DEFINE_CAT_CMD_ACTN("TM"_cat,  nullptr,                     cat_TM, "",          0, 14); // set Teensy RTC
DEFINE_CAT_CMD_PROP("VO"_cat,  &t41.AudioVolume,            cat_VO, "VO%03d;",   3,  6); // read/set volume
DEFINE_CAT_CMD_PROP("ZM"_cat,  &t41.SpectrumZoom,           cat_ZM, "ZM%d;",     3,  4); // read/set spectrum zoom

// wsjt-x specific commands
// *** the use of the wsjtCAT prefix below is really only needed for those commands also used above (ID here) ***
DEFINE_CAT_CMD_RO("AI"_cat, &t41.wsjtAI, wsjtCat_AI, "AI%d;",        3); // auto information
DEFINE_CAT_CMD_RO("ID"_cat, &t41.wsjtID, wsjtCat_ID, "ID%03d;",      3); // radio ID
DEFINE_CAT_CMD_RO("KS"_cat, &t41.wsjtKS, wsjtCat_KS, "KS%03d;",       3); // key speed
DEFINE_CAT_CMD_RO("PS"_cat, &t41.wsjtPS, wsjtCat_PS, "PS%d;",        3); // power
DEFINE_CAT_COMMAND("RX"_cat,             wsjtCat_RX, "",         0,  3); // RX
DEFINE_CAT_CMD_RO("SF"_cat, nullptr,     wsjtCat_SF, "SF%d%011d%d;", 4); // VFO freq and mode
DEFINE_CAT_CMD_RO("SP"_cat, &t41.wsjtSP, wsjtCat_SP, "SP%d;",        3); // split VFO
DEFINE_CAT_CMD_RO("TB"_cat, &t41.wsjtTB, wsjtCat_TB, "TB%d;",        3); // split
DEFINE_CAT_COMMAND("TX"_cat,             wsjtCat_TX, "",         0,  3); // TX

// build the command tables
// *** CATCommand structure placed at hash index of CAT command ***
struct RemoteCommandTable {
  const CATCommand* data[128];

  constexpr RemoteCommandTable() : data{} {
    data["BD"_cath] = &cat_BD_cmd;
    data["BU"_cath] = &cat_BU_cmd;
    data["DP"_cath] = &cat_DP_cmd;
    data["DS"_cath] = &cat_DS_cmd;
    data["FA"_cath] = &cat_FA_cmd;
    data["FB"_cath] = &cat_FB_cmd;
    data["FC"_cath] = &cat_FC_cmd;
    data["FF"_cath] = &cat_FF_cmd;
    data["FS"_cath] = &cat_FS_cmd;
    data["FT"_cath] = &cat_FT_cmd;
    data["F0"_cath] = &cat_F0_cmd;
    data["F1"_cath] = &cat_F1_cmd;
    data["GT"_cath] = &cat_GT_cmd;
    data["ID"_cath] = &cat_ID_cmd;
    data["IF"_cath] = &cat_IF_cmd;
    data["MD"_cath] = &cat_MD_cmd;
    data["ME"_cath] = &cat_ME_cmd;
    data["NF"_cath] = &cat_NF_cmd;
    data["NG"_cath] = &cat_NG_cmd;
    data["NH"_cath] = &cat_NH_cmd;
    data["NL"_cath] = &cat_NL_cmd;
    data["NS"_cath] = &cat_NS_cmd;
    data["NW"_cath] = &cat_NW_cmd;
    data["N1"_cath] = &cat_N1_cmd;
    data["PC"_cath] = &cat_PC_cmd;
    data["PG"_cath] = &cat_PG_cmd;
    data["SM"_cath] = &cat_SM_cmd;
    data["TM"_cath] = &cat_TM_cmd;
    data["VO"_cath] = &cat_VO_cmd;
    data["ZM"_cath] = &cat_ZM_cmd;
  }
};

struct WSJTCommandBuilder {
  const CATCommand* data[128];

  constexpr WSJTCommandBuilder() : data{} {
    data["AI"_cath] = &wsjtCat_AI_cmd;
    data["FA"_cath] = &cat_FA_cmd;
    data["FB"_cath] = &cat_FB_cmd;
    data["FT"_cath] = &cat_FT_cmd;
    data["ID"_cath] = &wsjtCat_ID_cmd;
    data["IF"_cath] = &cat_IF_cmd;
    data["KS"_cath] = &wsjtCat_KS_cmd;
    data["MD"_cath] = &cat_MD_cmd;
    data["PS"_cath] = &wsjtCat_PS_cmd;
    data["RX"_cath] = &wsjtCat_RX_cmd;
    data["SF"_cath] = &wsjtCat_SF_cmd;
    data["SP"_cath] = &wsjtCat_SP_cmd;
    data["TB"_cath] = &wsjtCat_TB_cmd;
    data["TM"_cath] = &cat_TM_cmd;
    data["TX"_cath] = &wsjtCat_TX_cmd;
  }
};

// *** this is declared here after the CAT command tables have been constructed ***

// *** use of __attribute__((section(".progmem.data") vs PROGMEM resolves MethodName##_Wrapper
//     section type conflict with catCommands set as PROGMEM or can use PROGMEM here and use
//     __attribute__((section(".progmem.data") on catCommands ***
static const RemoteCommandTable catCommands __attribute__((section(".progmem.data")));
static const WSJTCommandBuilder wsjtCommands __attribute__((section(".progmem.data")));

#if T41_WSJT_CAT_AUDIO
CatControl wsjtControl(&wsjtCommands.data[0], &Serial);
#endif
CatControl catControl(&catCommands.data[0]);
