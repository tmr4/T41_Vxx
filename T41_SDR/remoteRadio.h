#pragma once

#include "catControl.h"

/*

Classes derived from CatControl define the specific CAT commands supported
by the child class.  These are defined in functions of the form:

   void supportedCommand(const char* cmd) {}

and a helping macro of the form (defined in catControl.H):

  DEFINE_CAT_COMMAND(childClass, supportedCommand, token, readFormatStr, readLen, setLen);

Finally, two CAT command tables are created with pointers to them passed to the
parent class on construction. See the bottom of radio.cpp for an example table.

*/

//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

class RemoteRadio : public CatControl {
public:
  RemoteRadio(const CATCommand* const *cat, const CATCommand* const *wsjt) : CatControl(cat, wsjt) {}
  RemoteRadio(const CATCommand* const *cat) : CatControl(cat) {}
  //virtual ~RemoteRadio() {}

  virtual void ackIdReceipt() {}

private:
  //-------------------------------------------------------------------------------------------------------------
  // RemoteRadio - PC or remote unit control commands
  //-------------------------------------------------------------------------------------------------------------

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

public:
  // command table construction helpers
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

  DEFINE_CAT_COMMAND(RemoteRadio, wsjt_AI, "AI"_cat, "AI0;",         3,  0);
  DEFINE_CAT_COMMAND(RemoteRadio, wsjt_FA, "FA"_cat, "FA%011d;",     3, 14);
  DEFINE_CAT_COMMAND(RemoteRadio, wsjt_FB, "FB"_cat, "FB%011d;",     3, 14);
  DEFINE_CAT_COMMAND(RemoteRadio, wsjt_FT, "FT"_cat, "FT0;",         3,  4);
  DEFINE_CAT_COMMAND(RemoteRadio, wsjt_KS, "KS"_cat, "KS0%d;",       3,  0);
  DEFINE_CAT_COMMAND(RemoteRadio, wsjt_SF, "SF"_cat, "SF%d%011d%d;", 4,  0);
  DEFINE_CAT_COMMAND(RemoteRadio, wsjt_SP, "SP"_cat, "SP%d;",        3,  4);
  DEFINE_CAT_COMMAND(RemoteRadio, wsjt_TB, "TB"_cat, "TB%d;",        3,  4);
  DEFINE_CAT_COMMAND(RemoteRadio, wsjt_TX, "TX"_cat, "",             0,  3);
};
