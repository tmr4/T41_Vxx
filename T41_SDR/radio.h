#pragma once

#include "catControl.h"

/*

Classes derived from CatControl define the specific CAT commands supported
by the child class.  These are defined in functions of the form:

   void supportedCommand(const char* cmd, bool isRead) {}

and a helping macro of the form (defined in catControl.H):

    DEFINE_CAT_ACTION(childClass, supportedCommand);

Finally, the child class must define two members that will be passed to the
parent class on construction: catCommands, an array of CATCommand, one for each
CAT command the class supports, and CMD_COUNT, the total number of commands supported.

The catCommands array is initialized as follows:

const CATCommand RemoteRadio::catCommands[] = {
  {"XX"_cat, readLength,  setLength, childClass::supportedCommand_Wrapper},
  ...,
  ...
};

where:
  "XX" is a 2 character CAT command supported by the class

  _cat required helper function to turn convert 2 character CAT commands into a uint16_t

  readLength is the length of the read CAT command, including the required terminating semicolon.
  setLength is the length of the set CAT command, including the required terminating semicolon.
  *** set the command length to 0 if read or set isn't supported for the command ***

  childClass::supportedCommand is the fully qualified method associated with the CAT command

  _Wrapper is a helper macro that creates the method the parent will call to execute
           supportedCommand in response to a received CAT command

*/

//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

//struct RemoteCommandTable;
//extern const RemoteCommandTable catCommands;
//struct WSJTCommandBuilder;
//extern const WSJTCommandBuilder wsjtCommands;
// for (at bottom of file)
//static inline constexpr RemoteRadio::RemoteCommandTable catCommands PROGMEM {};
//static inline constexpr RemoteRadio::WSJTCommandBuilder wsjtCommands PROGMEM {};

//-------------------------------------------------------------------------------------------------------------
// RemoteRadio - PC or remote unit control commands
//-------------------------------------------------------------------------------------------------------------

class RemoteRadio : public CatControl {
private:
  void cat_BD(const char* cmd);   // band down
  void cat_BU(const char* cmd);   // band up
  void cat_DP(const char* cmd);   // pause data transfer
  void cat_DS(const char* cmd);   // start data transfer
  void cat_FA(const char* cmd);   // read/set VFO A frequency
  void cat_FB(const char* cmd);   // read/set VFO B frequency
  void cat_FC(const char* cmd);   // read/set current VFO center frequency
  void cat_FF(const char* cmd);   // read/set NCO frequency offset
  void cat_FS(const char* cmd);   // toggle fine tune status
  void cat_FT(const char* cmd);   // set VFO A or B
  void cat_F0(const char* cmd);   // set center or fine tune increment change
  void cat_F1(const char* cmd);   // set center or fine tune increment change
  void cat_GT(const char* cmd);   // read/set AGC
  void cat_ID(const char* cmd);   // read radio ID
  void cat_IF(const char* cmd);   // read transceiver status
  void cat_MD(const char* cmd);   // read/set demod mode
  void cat_ME(const char* cmd);   // read/set operating mode
  void cat_NF(const char* cmd);   // read/set noise floor
  void cat_NG(const char* cmd);   // set live noise floor
  void cat_NH(const char* cmd);   // set high audio filter frequency
  void cat_NL(const char* cmd);   // set low audio filter frequency
  void cat_NS(const char* cmd);   // inc/dec audio filter
  void cat_NW(const char* cmd);   // set 0.5kHz-1.5kHz audio filter
  void cat_N1(const char* cmd);   // set noise filter
  void cat_PC(const char* cmd);   // read/set transmit power level
  void cat_PG(const char* cmd);   // read/set RF gain
  void cat_SM(const char* cmd);   // read S-meter
  void cat_TM(const char* cmd);   // set Teensy RTC
  void cat_VO(const char* cmd);   // read/set volume
  void cat_ZM(const char* cmd);   // read/set spectrum zoom

public:

  // DEFINE_CAT_COMMAND use:
  // Example for FA command:
  // DEFINE_CAT_COMMAND(CatControl, cat_FA, "FA%010d;", 13, 3);
  // This defines fmt_cat_FA, Action_cat_FA, and cat_FA_cmd
  // The first two are used in the macro itself, the last is
  // used in creating the command table:
  //  const CATCommand RemoteRadio::catCommands[] = {
  //   table.data[idx] = &cat_FA_cmd;
  //   ...
  //  };
  DEFINE_CAT_COMMAND(RemoteRadio, cat_BD, "BD"_cat, "",          3,  4);
  DEFINE_CAT_COMMAND(RemoteRadio, cat_BU, "BU"_cat, "",          3,  4);
  DEFINE_CAT_COMMAND(RemoteRadio, cat_DP, "DP"_cat, "",          0,  3);
  DEFINE_CAT_COMMAND(RemoteRadio, cat_DS, "DS"_cat, "",          0,  3);
  DEFINE_CAT_COMMAND(RemoteRadio, cat_FA, "FA"_cat, "FA%011d;",  3, 14);
  DEFINE_CAT_COMMAND(RemoteRadio, cat_FB, "FB"_cat, "FB%011d;",  3, 14);
  DEFINE_CAT_COMMAND(RemoteRadio, cat_FC, "FC"_cat, "FC%011d;",  3, 14);
  DEFINE_CAT_COMMAND(RemoteRadio, cat_FF, "FF"_cat, "FF%011d;",  3, 14);
  DEFINE_CAT_COMMAND(RemoteRadio, cat_FS, "FS"_cat, "FS%d;",     3,  3);
  DEFINE_CAT_COMMAND(RemoteRadio, cat_FT, "FT"_cat, "",          0,  4);
  DEFINE_CAT_COMMAND(RemoteRadio, cat_F0, "F0"_cat, "F0%d;",     3,  4);
  DEFINE_CAT_COMMAND(RemoteRadio, cat_F1, "F1"_cat, "F1%d;",     3,  4);
  DEFINE_CAT_COMMAND(RemoteRadio, cat_GT, "GT"_cat, "GT%d;",     3,  4);
  DEFINE_CAT_COMMAND(RemoteRadio, cat_ID, "ID"_cat, "ID%03d;",   3,  6);
  DEFINE_CAT_COMMAND(RemoteRadio, cat_IF, "IF"_cat, "",          3,  0);
  DEFINE_CAT_COMMAND(RemoteRadio, cat_MD, "MD"_cat, "MD%d;",     3,  4);
  DEFINE_CAT_COMMAND(RemoteRadio, cat_ME, "ME"_cat, "ME%d;",     3,  4);
  DEFINE_CAT_COMMAND(RemoteRadio, cat_NF, "NF"_cat, "NF%04d;",   3,  7);
  DEFINE_CAT_COMMAND(RemoteRadio, cat_NG, "NG"_cat, "NG%d;",     3,  4);
  DEFINE_CAT_COMMAND(RemoteRadio, cat_NH, "NH"_cat, "NH%011d;",  3, 14);
  DEFINE_CAT_COMMAND(RemoteRadio, cat_NL, "NL"_cat, "NL%011d;",  3, 14);
  DEFINE_CAT_COMMAND(RemoteRadio, cat_NS, "NS"_cat, "",          0,  5);
  DEFINE_CAT_COMMAND(RemoteRadio, cat_NW, "NW"_cat, "",          0,  3);
  DEFINE_CAT_COMMAND(RemoteRadio, cat_N1, "N1"_cat, "N1%d;",     3,  4);
  DEFINE_CAT_COMMAND(RemoteRadio, cat_PC, "PC"_cat, "PC%02d;",   3,  5);
  DEFINE_CAT_COMMAND(RemoteRadio, cat_PG, "PG"_cat, "PG%+03d;",  3,  6);
  DEFINE_CAT_COMMAND(RemoteRadio, cat_SM, "SM"_cat, "SM0%+05d;", 3,  4);
  DEFINE_CAT_COMMAND(RemoteRadio, cat_TM, "TM"_cat, "",          0, 14);
  DEFINE_CAT_COMMAND(RemoteRadio, cat_VO, "VO"_cat, "VO%03d;",   3,  6);
  DEFINE_CAT_COMMAND(RemoteRadio, cat_ZM, "ZM"_cat, "ZM%d;",     3,  4);

  virtual void ackIdReceipt() {}

//-------------------------------------------------------------------------------------------------------------
// WSJT-X specific commands
//-------------------------------------------------------------------------------------------------------------

  void wsjt_AI(const char* cmd);   // auto information
  void wsjt_FA(const char* cmd);   // read/set VFO A frequency
  void wsjt_FB(const char* cmd);   // read/set VFO B frequency
  void wsjt_FT(const char* cmd);   // set VFO A or B
  void wsjt_KS(const char* cmd);   // key speed
  void wsjt_SF(const char* cmd);   // read VFO freq and mode
  void wsjt_SP(const char* cmd);   // read split VFO
  void wsjt_TB(const char* cmd);   // read split
  void wsjt_TX(const char* cmd);   // TX

  DEFINE_CAT_COMMAND(RemoteRadio, wsjt_AI, "AI"_cat, "AI0;",         3,  0);
  DEFINE_CAT_COMMAND(RemoteRadio, wsjt_FA, "FA"_cat, "FA%011d;",     3, 14);
  DEFINE_CAT_COMMAND(RemoteRadio, wsjt_FB, "FB"_cat, "FB%011d;",     3, 14);
  DEFINE_CAT_COMMAND(RemoteRadio, wsjt_FT, "FT"_cat, "FT0;",         3,  4);
  DEFINE_CAT_COMMAND(RemoteRadio, wsjt_KS, "KS"_cat, "KS0%d;",       3,  0);
  DEFINE_CAT_COMMAND(RemoteRadio, wsjt_SF, "SF"_cat, "SF%d%011d%d;", 3,  0);
  DEFINE_CAT_COMMAND(RemoteRadio, wsjt_SP, "SP"_cat, "SP%d;",        3,  4);
  DEFINE_CAT_COMMAND(RemoteRadio, wsjt_TB, "TB"_cat, "TB%d;",        3,  4);
  DEFINE_CAT_COMMAND(RemoteRadio, wsjt_TX, "TX"_cat, "",             0,  3);

public:
public:
  //const CATCommand* const *catCommands;
  //const CATCommand* const *wsjtCommands;

  //static const RemoteCommandTable catCommands;
  //static const WSJTCommandBuilder wsjtCommands;

public:
  //RemoteRadio(bool useWSJT = false) : CatControl(useWSJT ? wsjtCommands.data : catCommands.data, useWSJT) {}
  RemoteRadio(const CATCommand* const *cat, const CATCommand* const *wsjt, bool useWSJT = false) : CatControl(useWSJT ? wsjt : cat, useWSJT) {}
  //virtual ~RemoteRadio() {}

protected:

};

// can't have static here
//static inline constexpr RemoteRadio::RemoteCommandTable RemoteRadio::catCommands PROGMEM {};
//static inline constexpr RemoteRadio::WSJTCommandBuilder RemoteRadio::wsjtCommands PROGMEM {};

//static inline constexpr RemoteRadio::RemoteCommandTable catCommands PROGMEM {};
//static inline constexpr RemoteRadio::WSJTCommandBuilder wsjtCommands PROGMEM {};

//inline const RemoteRadio::RemoteCommandTable RemoteRadio::catCommands PROGMEM {};
//inline const RemoteRadio::WSJTCommandBuilder RemoteRadio::wsjtCommands PROGMEM {};



/*
AI suggested fix for table problem

// In your .h or .cpp where the table is defined:
static constexpr auto build_kenwood_table() {
    std::array<const CommandStruct*, 128> table{}; // Ensures zeros

    // Explicitly set the 36 indices
    table[5]   = &RemoteRadio::cat_ID_cmd;  // ID
    table[111] = &RemoteRadio::cat_FA_cmd;  // FA
    table[52]  = &RemoteRadio::cat_BU_cmd;  // BU
    // ...

    return table;
}

// In C++17, this is the most "bulletproof" way to get it into Flash
struct TableWrapper {
    const CommandStruct* const data[128];
};

static const TableWrapper kenwood_flash PROGMEM = {{
    #include "kenwood_indices.h" // Or just the raw list of [idx] = &cmd
}};

*/
