
#include "catControl.h"

//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

//-------------------------------------------------------------------------------------------------------------
// RemoteRadio - PC or remote unit control commands
//-------------------------------------------------------------------------------------------------------------

class RemoteRadio : public CatControl {
private:
  static constexpr size_t CMD_COUNT = 28;
  static const CATCommand catCommands[CMD_COUNT];

public:
  RemoteRadio() : CatControl(catCommands, CMD_COUNT) {}
  //virtual ~RemoteRadio() {}

protected:

private:
  void handleDP(const char* cmd, bool isRead);
  void handleDS(const char* cmd, bool isRead);
  void handleFF(const char* cmd, bool isRead);
  void handleFI(const char* cmd, bool isRead);
  void handleFS(const char* cmd, bool isRead);
  void handleFT(const char* cmd, bool isRead);
  void handleGT(const char* cmd, bool isRead);
  void handleIF(const char* cmd, bool isRead);
  void handleMD(const char* cmd, bool isRead);
  void handleME(const char* cmd, bool isRead);
  void handleNF(const char* cmd, bool isRead);
  void handleNG(const char* cmd, bool isRead);
  void handleNH(const char* cmd, bool isRead);
  void handleNL(const char* cmd, bool isRead);
  void handleNS(const char* cmd, bool isRead);
  void handleNW(const char* cmd, bool isRead);
  void handleN1(const char* cmd, bool isRead);
  void handlePC(const char* cmd, bool isRead);
  void handleSM(const char* cmd, bool isRead);
  void handleVO(const char* cmd, bool isRead);
  void handleZM(const char* cmd, bool isRead);

public:
  DEFINE_CAT_ACTION(RemoteRadio, handleBD);
  DEFINE_CAT_ACTION(RemoteRadio, handleBU);
  DEFINE_CAT_ACTION(RemoteRadio, handleDP);
  DEFINE_CAT_ACTION(RemoteRadio, handleDS);
  DEFINE_CAT_ACTION(RemoteRadio, handleFA);
  DEFINE_CAT_ACTION(RemoteRadio, handleFB);
  DEFINE_CAT_ACTION(RemoteRadio, handleFC);
  DEFINE_CAT_ACTION(RemoteRadio, handleFF);
  DEFINE_CAT_ACTION(RemoteRadio, handleFI);
  DEFINE_CAT_ACTION(RemoteRadio, handleFS);
  DEFINE_CAT_ACTION(RemoteRadio, handleFT);
  DEFINE_CAT_ACTION(RemoteRadio, handleGT);
  DEFINE_CAT_ACTION(RemoteRadio, handleID);
  DEFINE_CAT_ACTION(RemoteRadio, handleIF);
  DEFINE_CAT_ACTION(RemoteRadio, handleMD);
  DEFINE_CAT_ACTION(RemoteRadio, handleME);
  DEFINE_CAT_ACTION(RemoteRadio, handleNF);
  DEFINE_CAT_ACTION(RemoteRadio, handleNG);
  DEFINE_CAT_ACTION(RemoteRadio, handleNH);
  DEFINE_CAT_ACTION(RemoteRadio, handleNL);
  DEFINE_CAT_ACTION(RemoteRadio, handleNS);
  DEFINE_CAT_ACTION(RemoteRadio, handleNW);
  DEFINE_CAT_ACTION(RemoteRadio, handleN1);
  DEFINE_CAT_ACTION(RemoteRadio, handlePC);
  DEFINE_CAT_ACTION(RemoteRadio, handleSM);
  DEFINE_CAT_ACTION(RemoteRadio, handleTM);
  DEFINE_CAT_ACTION(RemoteRadio, handleVO);
  DEFINE_CAT_ACTION(RemoteRadio, handleZM);
};

//-------------------------------------------------------------------------------------------------------------
// WSJTXRadio - WSJT-X specific commands
//-------------------------------------------------------------------------------------------------------------
// WSJTXRadio impliments CAT control for WSJT-X.  It models the very limited subset of CAT commands used by WSJT-X for the Kenwood TS-890-S.
// Note that WSJTXRadio reflects the WSJT-X implimentation, not Kenwood's.  It's actually closer to the Kenwood TS-2000.
// WSJTXRadio isn't appropriate if you need true TS-890S CAT control.
//
// WSJTXRadio can also be used with the T41Sever app with the DX Lab Suite Commander selected as the rig.
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
   WSJTXRadio impliments the following CAT commands:
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

class WSJTXRadio : public CatControl {
private:
  static constexpr size_t CMD_COUNT = 13;
  static const CATCommand catCommands[CMD_COUNT];

public:
  WSJTXRadio() : CatControl(catCommands, CMD_COUNT) {}

protected:

private:
  int mode = 2; // FT8 mode is always USB

  void handleAI(const char* cmd, bool isRead);
  void handleFA(const char* cmd, bool isRead);
  void handleFB(const char* cmd, bool isRead);
  void handleFT(const char* cmd, bool isRead);
  void handleID(const char* cmd, bool isRead) override;
  void handleIF(const char* cmd, bool isRead);
  void handleKS(const char* cmd, bool isRead);
  void handleMD(const char* cmd, bool isRead);
  void handleSF(const char* cmd, bool isRead);
  void handleSP(const char* cmd, bool isRead);
  void handleTB(const char* cmd, bool isRead);
  void handleTX(const char* cmd, bool isRead);

  DEFINE_CAT_ACTION(WSJTXRadio, handleAI);
  DEFINE_CAT_ACTION(WSJTXRadio, handleFA);
  DEFINE_CAT_ACTION(WSJTXRadio, handleFB);
  DEFINE_CAT_ACTION(WSJTXRadio, handleFT);
  DEFINE_CAT_ACTION(WSJTXRadio, handleID);
  DEFINE_CAT_ACTION(WSJTXRadio, handleIF);
  DEFINE_CAT_ACTION(WSJTXRadio, handleKS);
  DEFINE_CAT_ACTION(WSJTXRadio, handleMD);
  DEFINE_CAT_ACTION(WSJTXRadio, handleSF);
  DEFINE_CAT_ACTION(WSJTXRadio, handleSP);
  DEFINE_CAT_ACTION(WSJTXRadio, handleTB);
  DEFINE_CAT_ACTION(WSJTXRadio, handleTM);
  DEFINE_CAT_ACTION(WSJTXRadio, handleTX);
};

//-------------------------------------------------------------------------------------------------------------
// Code
//-------------------------------------------------------------------------------------------------------------
