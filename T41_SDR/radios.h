
#include "catControl.h"
#include "radios.h"

//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

//-------------------------------------------------------------------------------------------------------------
// KenwoodRadio
//-------------------------------------------------------------------------------------------------------------

class KenwoodRadio : public CatControl {
private:
  static const CommandEntry dispatchTable[];

protected:
  const CommandEntry* getDispatchTable() override { return dispatchTable; }
  size_t getTableSize() override { return sizeof(dispatchTable) / sizeof(CommandEntry); }

  int GetMode();

private:
  /*
  // radio-specific handlers
  void handleID(const char* cmd) {
      link->print("ID019;");
      // ... switch to FT8 logic ...
  }
  */
};

//-------------------------------------------------------------------------------------------------------------
// RemoteRadio - PC or remote unit control commands
//-------------------------------------------------------------------------------------------------------------

class RemoteRadio : public CatControl {
private:
  static const CommandEntry dispatchTable[];

protected:
  const CommandEntry* getDispatchTable() override { return dispatchTable; }
  size_t getTableSize() override { return sizeof(dispatchTable) / sizeof(CommandEntry); }

private:
  /*
  // radio-specific handlers
  void handleID(const char* cmd) {
      link->print("ID019;");
      // ... switch to FT8 logic ...
  }
  */
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

// *** probably need CatControl after KenwoodRadio is fleshed out ***
//class WSJTXRadio : public CatControl {
class WSJTXRadio : public KenwoodRadio {
private:
  static const CommandEntry dispatchTable[];

protected:
  const CommandEntry* getDispatchTable() override { return dispatchTable; }
  size_t getTableSize() override { return sizeof(dispatchTable) / sizeof(CommandEntry); }

private:
  int mode = 2; // FT8 mode is always USB

  void handleID(const char* cmd, const size_t len) override;
};


//-------------------------------------------------------------------------------------------------------------
// Code
//-------------------------------------------------------------------------------------------------------------
