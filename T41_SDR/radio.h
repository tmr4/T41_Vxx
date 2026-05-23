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

//-------------------------------------------------------------------------------------------------------------
// RemoteRadio - PC or remote unit control commands
//-------------------------------------------------------------------------------------------------------------

class RemoteRadio : public CatControl {
private:
  static constexpr size_t CMD_COUNT = 30;
  static const CATCommand catCommands[CMD_COUNT];
  static constexpr size_t WSJT_COUNT = 13;
  static const CATCommand wsjtCommands[WSJT_COUNT];

public:
  RemoteRadio(bool useWSJT = false) : CatControl(useWSJT ? wsjtCommands : catCommands, useWSJT ? WSJT_COUNT : CMD_COUNT) {}
  //virtual ~RemoteRadio() {}

protected:

private:
  void cat_BD(const char* cmd, bool isRead);
  void cat_BU(const char* cmd, bool isRead);
  void cat_DP(const char* cmd, bool isRead);
  void cat_DS(const char* cmd, bool isRead);
  void cat_FA(const char* cmd, bool isRead);
  void cat_FB(const char* cmd, bool isRead);
  void cat_FC(const char* cmd, bool isRead);
  void cat_FF(const char* cmd, bool isRead);
  void cat_FS(const char* cmd, bool isRead);
  void cat_FT(const char* cmd, bool isRead);
  void cat_F0(const char* cmd, bool isRead);
  void cat_F1(const char* cmd, bool isRead);
  void cat_GT(const char* cmd, bool isRead);
  void cat_ID(const char* cmd, bool isRead);
  void cat_IF(const char* cmd, bool isRead);
  void cat_MD(const char* cmd, bool isRead);
  void cat_ME(const char* cmd, bool isRead);
  void cat_NF(const char* cmd, bool isRead);
  void cat_NG(const char* cmd, bool isRead);
  void cat_NH(const char* cmd, bool isRead);
  void cat_NL(const char* cmd, bool isRead);
  void cat_NS(const char* cmd, bool isRead);
  void cat_NW(const char* cmd, bool isRead);
  void cat_N1(const char* cmd, bool isRead);
  void cat_PC(const char* cmd, bool isRead);
  void cat_PG(const char* cmd, bool isRead);
  void cat_SM(const char* cmd, bool isRead);
  void cat_TM(const char* cmd, bool isRead);
  void cat_VO(const char* cmd, bool isRead);
  void cat_ZM(const char* cmd, bool isRead);

  DEFINE_CAT_ACTION(RemoteRadio, cat_BD);
  DEFINE_CAT_ACTION(RemoteRadio, cat_BU);
  DEFINE_CAT_ACTION(RemoteRadio, cat_DP);
  DEFINE_CAT_ACTION(RemoteRadio, cat_DS);
  DEFINE_CAT_ACTION(RemoteRadio, cat_FA);
  DEFINE_CAT_ACTION(RemoteRadio, cat_FB);
  DEFINE_CAT_ACTION(RemoteRadio, cat_FC);
  DEFINE_CAT_ACTION(RemoteRadio, cat_FF);
  DEFINE_CAT_ACTION(RemoteRadio, cat_FS);
  DEFINE_CAT_ACTION(RemoteRadio, cat_FT);
  DEFINE_CAT_ACTION(RemoteRadio, cat_F0);
  DEFINE_CAT_ACTION(RemoteRadio, cat_F1);
  DEFINE_CAT_ACTION(RemoteRadio, cat_GT);
  DEFINE_CAT_ACTION(RemoteRadio, cat_ID);
  DEFINE_CAT_ACTION(RemoteRadio, cat_IF);
  DEFINE_CAT_ACTION(RemoteRadio, cat_MD);
  DEFINE_CAT_ACTION(RemoteRadio, cat_ME);
  DEFINE_CAT_ACTION(RemoteRadio, cat_NF);
  DEFINE_CAT_ACTION(RemoteRadio, cat_NG);
  DEFINE_CAT_ACTION(RemoteRadio, cat_NH);
  DEFINE_CAT_ACTION(RemoteRadio, cat_NL);
  DEFINE_CAT_ACTION(RemoteRadio, cat_NS);
  DEFINE_CAT_ACTION(RemoteRadio, cat_NW);
  DEFINE_CAT_ACTION(RemoteRadio, cat_N1);
  DEFINE_CAT_ACTION(RemoteRadio, cat_PC);
  DEFINE_CAT_ACTION(RemoteRadio, cat_PG);
  DEFINE_CAT_ACTION(RemoteRadio, cat_SM);
  DEFINE_CAT_ACTION(RemoteRadio, cat_TM);
  DEFINE_CAT_ACTION(RemoteRadio, cat_VO);
  DEFINE_CAT_ACTION(RemoteRadio, cat_ZM);

  virtual void ackIdReceipt() {}

//-------------------------------------------------------------------------------------------------------------
// WSJT-X
//-------------------------------------------------------------------------------------------------------------

int mode = 2; // FT8 mode is always USB

  void wsjt_AI(const char* cmd, bool isRead);
  void wsjt_FA(const char* cmd, bool isRead);
  void wsjt_FB(const char* cmd, bool isRead);
  void wsjt_FT(const char* cmd, bool isRead);
  void wsjt_ID(const char* cmd, bool isRead);
  void wsjt_IF(const char* cmd, bool isRead);
  void wsjt_KS(const char* cmd, bool isRead);
  void wsjt_MD(const char* cmd, bool isRead);
  void wsjt_SF(const char* cmd, bool isRead);
  void wsjt_SP(const char* cmd, bool isRead);
  void wsjt_TB(const char* cmd, bool isRead);
  void wsjt_TX(const char* cmd, bool isRead);

  DEFINE_CAT_ACTION(RemoteRadio, wsjt_AI);
  DEFINE_CAT_ACTION(RemoteRadio, wsjt_FA);
  DEFINE_CAT_ACTION(RemoteRadio, wsjt_FB);
  DEFINE_CAT_ACTION(RemoteRadio, wsjt_FT);
  DEFINE_CAT_ACTION(RemoteRadio, wsjt_ID);
  DEFINE_CAT_ACTION(RemoteRadio, wsjt_IF);
  DEFINE_CAT_ACTION(RemoteRadio, wsjt_KS);
  DEFINE_CAT_ACTION(RemoteRadio, wsjt_MD);
  DEFINE_CAT_ACTION(RemoteRadio, wsjt_SF);
  DEFINE_CAT_ACTION(RemoteRadio, wsjt_SP);
  DEFINE_CAT_ACTION(RemoteRadio, wsjt_TB);
  DEFINE_CAT_ACTION(RemoteRadio, wsjt_TX);

};
