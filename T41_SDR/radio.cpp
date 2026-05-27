
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

// *** comments below show both read/set command structure, but these functions
//     only cover set commands ***
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
  heatbeart = millis(); // note time for heartbeat
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

/*
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
*/
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
  ChangeBand(f);
  t41.SetFreqA(f);
}

// read/set VFO B frequency
// "FB;" (length 3) or "FBxxxxxxxxxxx;" (length 14)
void RemoteRadio::wsjt_FB(const char* cmd) {
  long f = atol(&cmd[2]);
  ChangeBand(f);
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
//class RemoteCommandTable {
//public:
  //const CATCommand* data[128] = {};
  const CATCommand* data[128];
  //constexpr RemoteCommandTable();
};

//static inline constexpr RemoteCommandTable() : data{} {
//constexpr RemoteCommandTable::RemoteCommandTable() : data{
static constexpr RemoteCommandTable makeCatTable() {
  RemoteCommandTable wrapper = {};
  /*
    wrapper.data[get_cat_index("BD"_cat)] = &RemoteRadio::cat_BD_cmd;
    wrapper.data[get_cat_index("BU"_cat)] = &RemoteRadio::cat_BU_cmd;
    wrapper.data[get_cat_index("DP"_cat)] = &RemoteRadio::cat_DP_cmd;
    wrapper.data[get_cat_index("DS"_cat)] = &RemoteRadio::cat_DS_cmd;
    wrapper.data[get_cat_index("FA"_cat)] = &RemoteRadio::cat_FA_cmd;
    wrapper.data[get_cat_index("FB"_cat)] = &RemoteRadio::cat_FB_cmd;
    wrapper.data[get_cat_index("FC"_cat)] = &RemoteRadio::cat_FC_cmd;
    wrapper.data[get_cat_index("FF"_cat)] = &RemoteRadio::cat_FF_cmd;
    wrapper.data[get_cat_index("FS"_cat)] = &RemoteRadio::cat_FS_cmd;
    wrapper.data[get_cat_index("FT"_cat)] = &RemoteRadio::cat_FT_cmd;
    wrapper.data[get_cat_index("F0"_cat)] = &RemoteRadio::cat_F0_cmd;
    wrapper.data[get_cat_index("F1"_cat)] = &RemoteRadio::cat_F1_cmd;
    wrapper.data[get_cat_index("GT"_cat)] = &RemoteRadio::cat_GT_cmd;
    wrapper.data[get_cat_index("ID"_cat)] = &RemoteRadio::cat_ID_cmd;
    wrapper.data[get_cat_index("IF"_cat)] = &RemoteRadio::cat_IF_cmd;
    wrapper.data[get_cat_index("MD"_cat)] = &RemoteRadio::cat_MD_cmd;
    wrapper.data[get_cat_index("ME"_cat)] = &RemoteRadio::cat_ME_cmd;
    wrapper.data[get_cat_index("NF"_cat)] = &RemoteRadio::cat_NF_cmd;
    wrapper.data[get_cat_index("NG"_cat)] = &RemoteRadio::cat_NG_cmd;
    wrapper.data[get_cat_index("NH"_cat)] = &RemoteRadio::cat_NH_cmd;
    wrapper.data[get_cat_index("NL"_cat)] = &RemoteRadio::cat_NL_cmd;
    wrapper.data[get_cat_index("NS"_cat)] = &RemoteRadio::cat_NS_cmd;
    wrapper.data[get_cat_index("NW"_cat)] = &RemoteRadio::cat_NW_cmd;
    wrapper.data[get_cat_index("N1"_cat)] = &RemoteRadio::cat_N1_cmd;
    wrapper.data[get_cat_index("PC"_cat)] = &RemoteRadio::cat_PC_cmd;
    wrapper.data[get_cat_index("PG"_cat)] = &RemoteRadio::cat_PG_cmd;
    wrapper.data[get_cat_index("SM"_cat)] = &RemoteRadio::cat_SM_cmd;
    wrapper.data[get_cat_index("TM"_cat)] = &RemoteRadio::cat_TM_cmd;
    wrapper.data[get_cat_index("VO"_cat)] = &RemoteRadio::cat_VO_cmd;
    wrapper.data[get_cat_index("ZM"_cat)] = &RemoteRadio::cat_ZM_cmd;
    */
    wrapper.data["BD"_cath] = &RemoteRadio::cat_BD_cmd;
    wrapper.data["BU"_cath] = &RemoteRadio::cat_BU_cmd;
    wrapper.data["DP"_cath] = &RemoteRadio::cat_DP_cmd;
    wrapper.data["DS"_cath] = &RemoteRadio::cat_DS_cmd;
    wrapper.data["FA"_cath] = &RemoteRadio::cat_FA_cmd;
    wrapper.data["FB"_cath] = &RemoteRadio::cat_FB_cmd;
    wrapper.data["FC"_cath] = &RemoteRadio::cat_FC_cmd;
    wrapper.data["FF"_cath] = &RemoteRadio::cat_FF_cmd;
    wrapper.data["FS"_cath] = &RemoteRadio::cat_FS_cmd;
    wrapper.data["FT"_cath] = &RemoteRadio::cat_FT_cmd;
    wrapper.data["F0"_cath] = &RemoteRadio::cat_F0_cmd;
    wrapper.data["F1"_cath] = &RemoteRadio::cat_F1_cmd;
    wrapper.data["GT"_cath] = &RemoteRadio::cat_GT_cmd;
    wrapper.data["ID"_cath] = &RemoteRadio::cat_ID_cmd;
    wrapper.data["IF"_cath] = &RemoteRadio::cat_IF_cmd;
    wrapper.data["MD"_cath] = &RemoteRadio::cat_MD_cmd;
    wrapper.data["ME"_cath] = &RemoteRadio::cat_ME_cmd;
    wrapper.data["NF"_cath] = &RemoteRadio::cat_NF_cmd;
    wrapper.data["NG"_cath] = &RemoteRadio::cat_NG_cmd;
    wrapper.data["NH"_cath] = &RemoteRadio::cat_NH_cmd;
    wrapper.data["NL"_cath] = &RemoteRadio::cat_NL_cmd;
    wrapper.data["NS"_cath] = &RemoteRadio::cat_NS_cmd;
    wrapper.data["NW"_cath] = &RemoteRadio::cat_NW_cmd;
    wrapper.data["N1"_cath] = &RemoteRadio::cat_N1_cmd;
    wrapper.data["PC"_cath] = &RemoteRadio::cat_PC_cmd;
    wrapper.data["PG"_cath] = &RemoteRadio::cat_PG_cmd;
    wrapper.data["SM"_cath] = &RemoteRadio::cat_SM_cmd;
    wrapper.data["TM"_cath] = &RemoteRadio::cat_TM_cmd;
    wrapper.data["VO"_cath] = &RemoteRadio::cat_VO_cmd;
    wrapper.data["ZM"_cath] = &RemoteRadio::cat_ZM_cmd;
    return wrapper;
  }
/*
// 1. Define the array as a global constant with a flat initializer list
// This forces the linker to bake the actual addresses into the binary image
static const CATCommand* const catCommands[128] PROGMEM = {
//    [5]   = &RemoteRadio::cat_ID_cmd,
//    [10]  = &RemoteRadio::cat_ZM_cmd,
//    [52]  = &RemoteRadio::cat_BU_cmd,
//    [111] = &RemoteRadio::cat_FA_cmd,
//    [122] = &RemoteRadio::cat_BD_cmd
// 128-slot flat array using constant 40305
//static const CommandStruct* const kenwood_data[128] PROGMEM = {
    &RemoteRadio::cat_GT_cmd, nullptr, nullptr, nullptr, &RemoteRadio::cat_F1_cmd, &RemoteRadio::cat_ID_cmd, nullptr, &RemoteRadio::cat_DS_cmd, nullptr, nullptr,
    &RemoteRadio::cat_ZM_cmd, nullptr, &RemoteRadio::cat_NG_cmd, &RemoteRadio::cat_FC_cmd, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
    nullptr, &RemoteRadio::cat_NL_cmd, nullptr, nullptr, nullptr, nullptr, nullptr, &RemoteRadio::cat_DP_cmd, nullptr, nullptr,
    nullptr, nullptr, nullptr, nullptr, &RemoteRadio::cat_IF_cmd, nullptr, nullptr, nullptr, nullptr, nullptr,
    nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
    nullptr, nullptr, &RemoteRadio::cat_BU_cmd, &RemoteRadio::cat_F0_cmd, &RemoteRadio::cat_ME_cmd, &RemoteRadio::cat_TM_cmd, nullptr, nullptr, nullptr, nullptr,
    &RemoteRadio::cat_NS_cmd, &RemoteRadio::cat_NF_cmd, &RemoteRadio::cat_FB_cmd, nullptr, nullptr, nullptr, &RemoteRadio::cat_PC_cmd, nullptr, nullptr, &RemoteRadio::cat_VO_cmd,
    nullptr, &RemoteRadio::cat_FT_cmd, &RemoteRadio::cat_N1_cmd, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
    nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, &RemoteRadio::cat_NH_cmd,
    nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
    nullptr, nullptr, &RemoteRadio::cat_MD_cmd, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
    &RemoteRadio::cat_FA_cmd, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, &RemoteRadio::cat_NW_cmd, &RemoteRadio::cat_FS_cmd,
    &RemoteRadio::cat_FF_cmd, &RemoteRadio::cat_BD_cmd, nullptr, nullptr, &RemoteRadio::cat_PG_cmd, &RemoteRadio::cat_SM_cmd, nullptr
};
*/
// 2. The Radio declaration (at the very bottom)
// Now, when this constructor runs, 'kenwood_data' is not 'empty memory'
// it is a pre-baked block of pointers in Flash/RAM.
//RemoteRadio radio_instance(kenwood_data);

//struct WSJTCommandBuilder {
class WSJTCommandBuilder {
public:
  //const CATCommand* data[128] = {};
  const CATCommand* data[128];
  constexpr WSJTCommandBuilder();
};

//static inline constexpr WSJTCommandBuilder() : data{} {
constexpr WSJTCommandBuilder::WSJTCommandBuilder() : data{} {
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

//static inline constexpr RemoteCommandTable catCommands PROGMEM {};
//static inline constexpr WSJTCommandBuilder wsjtCommands PROGMEM {};
//static inline constexpr RemoteCommandTable catCommands PROGMEM = {};
//static inline constexpr WSJTCommandBuilder wsjtCommands PROGMEM = {};
//static inline constexpr RemoteCommandTable catCommands PROGMEM;
//static inline constexpr WSJTCommandBuilder wsjtCommands PROGMEM;
//static constexpr RemoteCommandTable catCommands PROGMEM;
//static constexpr WSJTCommandBuilder wsjtCommands PROGMEM;
static const RemoteCommandTable catCommands PROGMEM = makeCatTable();
constexpr WSJTCommandBuilder wsjtCommands;

RemoteRadio radio(&catCommands.data[0], &wsjtCommands.data[0]);
