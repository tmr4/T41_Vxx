
#include "catControl.h"
#include "radios.h"

//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

//-------------------------------------------------------------------------------------------------------------
// Code
//-------------------------------------------------------------------------------------------------------------

//-------------------------------------------------------------------------------------------------------------
// KenwoodRadio
//-------------------------------------------------------------------------------------------------------------

/*
void KenwoodRadio::handle(const char* cmd, const size_t len) {
  if(len == 3) {
  }
}
*/
  // T41 to Kenwood TS-2000 modes
  // *** TODO: verify when this is used ***
  int KenwoodRadio::GetMode() {
    // 1: LSB, 2: USB, 3: CW, 4: FM, 5: AM
    int mode;
    if(t41.RadioMode == CW_MODE) {
      mode=3;
    } else {
      switch(t41.DemodMode) {
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

// Kenwood Command Dispatch Table (both inherited and radio specific)
// *** TODO: separate these into common, Kenwood specific, and alternate radio commands ***
const CatControl::CommandEntry KenwoodRadio::dispatchTable[] = {
//  {"", (CatControl::CmdHandler)&KenwoodRadio::handle},
  {"BD", (CatControl::CmdHandler)&KenwoodRadio::handleBandDown},  // band down
  {"BU", (CatControl::CmdHandler)&KenwoodRadio::handleBandUp},    // band up
  {"FA", (CatControl::CmdHandler)&KenwoodRadio::handleFA},        // read/set VFO A frequency
  {"FB", (CatControl::CmdHandler)&KenwoodRadio::handleFB},        // read/set VFO B frequency
  {"FC", (CatControl::CmdHandler)&KenwoodRadio::handleFC},        // read/set current VFO center frequency
  {"ID", (CatControl::CmdHandler)&KenwoodRadio::handleID},        // read radio ID
};

//-------------------------------------------------------------------------------------------------------------
// RemoteRadio - PC or remote unit control commands
//-------------------------------------------------------------------------------------------------------------

void RemoteRadio::handleDataStart(const char* cmd, const size_t len) {
  if(len == 3) {
    // start sending spectrum data
    controlDataFlag = true;
  }
}

void RemoteRadio::handleDataPause(const char* cmd, const size_t len) {
  if(len == 3) {
    // stop sending spectrum data
    controlDataFlag = false;
  }
}

// read/set NCO frequency offset
// "FF;" (length 3) or "FFxxxxxxxxxxx;" (length 14)
void RemoteRadio::handleFF(const char* cmd, const size_t len) {
  if(len == 3) { // read
    snprintf(msg, sizeof(msg), "FF%011lu;", (long)t41.NCOFreq);
    send(msg);
  } else if(len == 14) { // set
    t41.NCOFreq.Update(atol(&cmd[2]));
  }
}

// set center or fine tune frequency increment change
// "FIx;" (length 4) x=0 center; x=1 fine tune
void RemoteRadio::handleFI(const char* cmd, const size_t len) {
  if(len == 4) {
    if(cmd[2] == '0') {
      ChangeFreqIncrement(atol(&cmd[3]) - t41.CenterTuneIndex, false);
    } else if(cmd[2] == '1') {
      ChangeFtIncrement(atol(&cmd[3]) - t41.FineTuneIndex, false);
    }
  }
}

// toggle fine tune status, on/off
// "FS;" (length 3)
void RemoteRadio::handleFS(const char* cmd, const size_t len) {
  if(len == 3) {
    t41.MouseCenterTuneActive.Update(!atoi(&cmd[2]));
    HighlightTuneInc();
  }
}

// set VFO A or B (non-standard, this is specific for transmit on Kenwood)
// "FTx;" (length 4) x=0 VFO A; x=1 VFO B
void RemoteRadio::handleFT(const char* cmd, const size_t len) {
  if(len == 4) {
    VFOSelect(atoi(&cmd[2]));
    // SendAS(); // PC control specific ???
  }
}

// read/set AGC (non-standard Kenwood command)
// "GTx;" (length 4) x=0 VFO A; x=1 VFO B
void RemoteRadio::handleGT(const char* cmd, const size_t len) {
  if(len == 3) {
    snprintf(msg, sizeof(msg), "GT%d;", t41.AGCMode);
    send(msg);
  } else if(len == 4) {
    t41.AGCMode.Update(atoi(&cmd[2]));
    UpdateInfoBoxItem(T41_ITEM_AGC);
  }
}

// read transceiver status
void RemoteRadio::handleIF(const char* cmd, const size_t len) {
  if(len == 3) {
    SendIF();
  }
}

// read/set demod mode (non-standard Kenwood TS-2000 command)
// "MD;" (length 3) or "MDx;" (length 4) x= demodulation mode (see SDT.h)
void RemoteRadio::handleMD(const char* cmd, const size_t len) {
  if(len == 3) {
    snprintf(msg, sizeof(msg), "MD%d;", t41.DemodMode);
    send(msg);
  } else if(len == 4) {
    ChangeDemodMode(atoi(&cmd[2]), false);
    // SendAS(); // PC control specific ???
  }
}

// read/set operating mode
// "ME;" (length 3) or "MEx;" (length 4) x= operating mode (see SDT.h)
void RemoteRadio::handleME(const char* cmd, const size_t len) {
  if(len == 3) {
    snprintf(msg, sizeof(msg), "MD%d;", t41.DemodMode);
    send(msg);
  } else if(len == 4) {
    ChangeMode(atoi(&cmd[2]), -1, false);
    // SendAS(); // PC control specific ???
  }
}

// *** TODO: some of these 'N' commands conflict with Kenwood commands
// read/set noise floor
// "NF;" (length 3) or "NFxxxx;" (length 7) xxxx= noise floor
void RemoteRadio::handleNF(const char* cmd, const size_t len) {
  if(len == 3) {
    snprintf(msg, sizeof(msg), "NF%04d;", (int)t41.NoiseFloor);
    send(msg);
  } else if(len == 7) {
    t41.NoiseFloor.Update(atoi(&cmd[2]));
  }
}

// set live noise floor
void RemoteRadio::handleNG(const char* cmd, const size_t len) {
  if(len == 4) {
    t41.LiveNoiseFloor.Update(atoi(&cmd[2]));
    UpdateInfoBoxItem(T41_ITEM_FLOOR);
  }
}

// set high audio filter frequency
void RemoteRadio::handleNH(const char* cmd, const size_t len) {
  if(len == 14) {
    t41.FilterHiCut.Update(atol(&cmd[2]));

    CalcAudioFilters();
  }
}

// set low audio filter frequency
void RemoteRadio::handleNL(const char* cmd, const size_t len) {
  if(len == 14) {
    t41.FilterLoCut.Update(atol(&cmd[2]));

    CalcAudioFilters();
  }
}

// inc/dec audio filter
void RemoteRadio::handleNS(const char* cmd, const size_t len) {
  if(len == 5) {
    posFilterEncoder += atoi(&cmd[2]);
    ProcessFilterEncoder();

    CalcAudioFilters();
    UpdateDisplayFilters();
  }
}

// set 0.5kHz-1.5kHz audio filter
void RemoteRadio::handleNW(const char* cmd, const size_t len) {
  if(len == 3) {
    t41.FilterLoCut.Update(500);
    t41.FilterHiCut.Update(1500);

    CalcAudioFilters();
  }
}

// set noise filter
void RemoteRadio::handleN1(const char* cmd, const size_t len) {
  if(len == 4) {
    t41.NoiseFilter.Update(atoi(&cmd[2]));
    UpdateInfoBoxItem(T41_ITEM_FILTER);
  }
}

// read/set transmit power level (non-standard Kenwood command)
// "PC;" (length 3) or "PCxx;" (length 5) xx= transmit power level
void RemoteRadio::handlePC(const char* cmd, const size_t len) {
  if(len == 3) {
    snprintf(msg, sizeof(msg), "PC%02d;", t41.TxPower);
    send(msg);
  } else if(len == 5) {
    t41.TxPower.Update(atoi(&cmd[2]));
    ShowCurrentPowerSetting();
  }
}

// read S-meter (non-standard Kenwood command)
// "SM;" (length 3)
// "SMx;" (length 4) x= 0: dbm; 1: S-meter
// "SMxyyyyy;" (length 9) x= see above; y= value
void RemoteRadio::handleSM(const char* cmd, const size_t len) {
  float32_t dbm = CalcSignalStrength();

  if(len == 3) {
    // One of the following:
    // send dBm
    //sprintf(cmd, "SM0%+05d;", (int)(dbm * 10));

    // send s-meter
    //sprintf(cmd, "SM20%04d;", smeterPad);

    // just send dBm for now
    snprintf(msg, sizeof(msg), "SM0%+05d;", (int)(dbm * 10));
    send(msg);
  } else if(len == 4) {
    int index = atoi(&cmd[2]);

    // just send dBm for now
    snprintf(msg, sizeof(msg), "SM%d%+05d;", index, (int)(dbm * 10));
  } else if(len == 9) {
    // One of the following:
    // SM0-xxxx; (receive dBm)
    //sprintf(cmd, "SM0%+05d;", (int)(dbm * 10));

    // send s-meter
    //sprintf(cmd, "SM20%04d;", smeterPad);

    //Serial.print("Received signal strength: ");
    signalStrengthReceivedIndex = atoi(&cmd[2]);
    signalStrength = ((float)atoi(&cmd[3])) / 10.0;
    signalStrengthReceived = true;
    //Serial.println(signalStrength);
  }
}

// set Teensy RTC
// "TMxxxxxxxxxxx;" (length 14)
void RemoteRadio::handleTM(const char* cmd, const size_t len) {
  if(len == 14) {
    Teensy3Clock.set(atol(&cmd[2]));
    setTime(atol(&cmd[2]));
  }
}

// read/set volume
// "VO;" (length 3) or "VOxxx;" (length 6) xxx= volume 0-100
void RemoteRadio::handleVO(const char* cmd, const size_t len) {
  if(len == 3) {
    snprintf(msg, sizeof(msg), "VO%03d;", t41.AudioVolume);
    send(msg);
  } else if(len == 6) {
    t41.AudioVolume.Update(atoi(&cmd[2]));
  }
}

// read/set spectrum zoom
// "ZM;" (length 3) or "ZMx;" (length 4) x= zoom (0 to MAX_ZOOM_ENTRIES - 1)
void RemoteRadio::(const char* cmd, const size_t len) {
  if(len == 3) {
    snprintf(msg, sizeof(msg), "ZM%d;", t41.SpectrumZoom);
    send(msg);
  } else if(len == 4) {
    t41.SpectrumZoom.Update(atoi(&cmd[2]));
  }
}

const CatControl::CommandEntry RemoteRadio::dispatchTable[] = {
  {"BD", (CatControl::CmdHandler)&RemoteRadio::handleBandDown},       // band down
  {"BU", (CatControl::CmdHandler)&RemoteRadio::handleBandUp},         // band up
  {"DS", (CatControl::CmdHandler)&RemoteRadio::handleDataStart},       // start data transfer
  {"DP", (CatControl::CmdHandler)&RemoteRadio::handleDataPause},       // pause data transfer
  {"FA", (CatControl::CmdHandler)&RemoteRadio::handleFA},             // read/set VFO A frequency
  {"FB", (CatControl::CmdHandler)&RemoteRadio::handleFB},             // read/set VFO B frequency
  {"FC", (CatControl::CmdHandler)&RemoteRadio::handleFC},             // read/set current VFO center frequency
  {"FF", (CatControl::CmdHandler)&RemoteRadio::handleFF},              // read/set NCO frequency offset
  {"FI", (CatControl::CmdHandler)&RemoteRadio::handleFI},              // set center or fine tune increment change
  {"FS", (CatControl::CmdHandler)&RemoteRadio::handleFS},              // toggle fine tune status
  {"FT", (CatControl::CmdHandler)&RemoteRadio::handleFT},              // set VFO A or B
  {"GT", (CatControl::CmdHandler)&RemoteRadio::handleGT},              // read/set AGC
  {"ID", (CatControl::CmdHandler)&RemoteRadio::handleID},             // read radio ID
  {"IF", (CatControl::CmdHandler)&RemoteRadio::handleIF},              // read transceiver status
  {"MD", (CatControl::CmdHandler)&RemoteRadio::handleMD},              // read/set demod mode
  {"MD", (CatControl::CmdHandler)&RemoteRadio::handleMD},              // read/set demod mode
  {"ME", (CatControl::CmdHandler)&RemoteRadio::handleME},              // read/set operating mode
  {"NF", (CatControl::CmdHandler)&RemoteRadio::handleNF},              // read/set noise floor
  {"NG", (CatControl::CmdHandler)&RemoteRadio::handleNG},              // set live noise floor
  {"NH", (CatControl::CmdHandler)&RemoteRadio::handleNH},              // set high audio filter frequency
  {"NL", (CatControl::CmdHandler)&RemoteRadio::handleNL},              // set low audio filter frequency
  {"NS", (CatControl::CmdHandler)&RemoteRadio::handleNS},              // inc/dec audio filter
  {"NW", (CatControl::CmdHandler)&RemoteRadio::handleNW},              // set 0.5kHz-1.5kHz audio filter
  {"N1", (CatControl::CmdHandler)&RemoteRadio::handleN1},              // set noise filter
  {"PC", (CatControl::CmdHandler)&RemoteRadio::handlePC},              // read/set transmit power level
  {"SM", (CatControl::CmdHandler)&RemoteRadio::handleSM},              // read S-meter
  {"TM", (CatControl::CmdHandler)&RemoteRadio::handleTM},              // set Teensy RTC
  {"VO", (CatControl::CmdHandler)&RemoteRadio::handleVO},              // read/set volume
  {"ZM", (CatControl::CmdHandler)&RemoteRadio::handleZM},              // read/set spectrum zoom
};

//-------------------------------------------------------------------------------------------------------------
// WSJTXRadio - WSJT-X specific commands
//-------------------------------------------------------------------------------------------------------------

// WSJT-X had trouble with Kenwood TS-2000 use the TS-890S instead
// WSJT-X doesn't model Kenwood TS-890S computer control commands, but
// rather uses a subset of TS-2000 commands.

// Auto Information
// "AI;" (length 3) or "AIx;" (length 4) x=0 off; x=1 on?
void WSJTXRadio::handleAI(const char* cmd, const size_t len) {
  if(len == 3) {
    snprintf(msg, sizeof(msg), "AI0;"); // Auto info off
    send(msg);
  } else if(len == 4) {
  }
}

// read/set VFO A frequency
// "FA;" (length 3) or "FAxxxxxxxxxxx;" (length 14)
void WSJTXRadio::handleFA(const char* cmd, const size_t len) override {
  if(len == 3) {
    snprintf(msg, sizeof(msg), "FA%011lu;", t41.GetFreqA());
    send(msg);
  } else if(len == 14) {
    long f = atol(&cmd[2]);
    ChangeBand(f);
    t41.SetFreqA(f);
  }
}

// read/set VFO B frequency
// "FB;" (length 3) or "FBxxxxxxxxxxx;" (length 14)
void WSJTXRadio::handleFB(const char* cmd, const size_t len) {
  if(len == 3) {
    snprintf(msg, sizeof(msg), "FB%011lu;", t41.GetFreqB());
    send(msg);
  } else if(len == 14) {
    long f = atol(&cmd[2]);
    ChangeBand(f);
    t41.SetFreqB(f);
  }
}

// read/set VFO A or B
// "FTx;" (length 4) x=0 VFO A; x=1 VFO B
void WSJTXRadio::handleFT(const char* cmd, const size_t len) {
  if(len == 3) {
    snprintf(msg, sizeof(msg), "FT0;"); // T41 always responds transmit on VFO A
    send(msg);
  } else if(len == 4) {
    VFOSelect(atoi(&cmd[2]));
  }
}

void WSJTXRadio::handleID(const char* cmd, const size_t len) override {
  if(len == 3) {
    // receipt of ID command will switch to FT8 Data mode if not already there
    ChangeMode(DATA_MODE, DEMOD_FT8);

    // reply with the TS-890S id
    snprintf(msg, sizeof(msg), "ID024;");
    //snprintf(msg, sizeof(msg), "ID019;"); // TS-2000
    send(msg);
  }
}

// read transceiver status
// "IF;" (length = 3)
void WSJTXRadio::handleIF(const char* cmd, const size_t len) override {
  if(len == 3) {
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
    sprintf(msg, sizeof(msg), "IF%011d%04d%+06d%d%d%d%02d%d%d%d%d%d%d%02d%d;",
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
    send(msg);
  }
}

// Keying Speed
// "KS;" (length 3)
void WSJTXRadio::handleKS(const char* cmd, const size_t len) {
  if(len == 3) {
    snprintf(msg, sizeof(msg), "KS0%d;", DEFAULT_KEYER_WPM);
    send(msg);
  }
}

// read demod mode
// "MD;" (length 3)
void WSJTXRadio::handleMD(const char* cmd, const size_t len) {
  if(len == 3) {
    snprintf(msg, sizeof(msg), "MD%d;", mode);
    send(msg);
  }
}

// read VFO frequency and mode
// "SF;" (length 3)
void WSJTXRadio::handleSF(const char* cmd, const size_t len) {
  if(len == 3) {
    int vfo = atoi(&cmd[2]);
    int freq = vfo == 0 ? t41.GetFreqA() : t41.GetFreqB();
    snprintf(msg, sizeof(msg), "SF%d%011d%d;", vfo, freq, mode);
    send(msg);
  } else if(len == 4 {
    // ignored
  }
}

// Split VFO
// "SP;" (length 3) or "SPx;" (length 4)
void WSJTXRadio::handleSP(const char* cmd, const size_t len) {
  if(len == 3) {
    snprintf(msg, sizeof(msg), "SP%d;", 0); // no split VFO
    send(msg);
  } else if(len == 4 {
    // ignored
  }
}

// Split
// "TB;" (length 3) or "TBx;" (length 4)
void WSJTXRadio::handleTB(const char* cmd, const size_t len) {
  if(len == 3) {
    snprintf(msg, sizeof(msg), "TB%d;", 0); // no split VFO
    send(msg);
  } else if(len == 4 {
    // ignored
  }
}

// TX
// "TX;" (length 3)
void WSJTXRadio::handleTX(const char* cmd, const size_t len) {
  if(len == 3) {
    ft8PTT = true;
  }
}

const CatControl::CommandEntry WSJTXRadio::dispatchTable[] = {
  {"DP", (CatControl::CmdHandler)&WSJTXRadio::handleAI},              // auto information
  {"FA", (CatControl::CmdHandler)&WSJTXRadio::handleFA},              // read/set VFO A frequency
  {"FB", (CatControl::CmdHandler)&WSJTXRadio::handleFB},              // read/set VFO B frequency
  {"FT", (CatControl::CmdHandler)&WSJTXRadio::handleFT},              // set VFO A or B
  {"ID", (CatControl::CmdHandler)&WSJTXRadio::handleID},              // read radio ID
  {"IF", (CatControl::CmdHandler)&WSJTXRadio::handleIF},              // read transceiver status
  {"KS", (CatControl::CmdHandler)&WSJTXRadio::handleKS},              // key speed
  {"MD", (CatControl::CmdHandler)&WSJTXRadio::handleMD},              // read/set demod mode
  {"SF", (CatControl::CmdHandler)&WSJTXRadio::handleSF},              // read VFO freq and mode
  {"SP", (CatControl::CmdHandler)&WSJTXRadio::handleSP},              // read split VFO
  {"TB", (CatControl::CmdHandler)&WSJTXRadio::handleTB},              // read split
  {"TM", (CatControl::CmdHandler)&RemoteRadio::handleTM},             // set Teensy RTC
  {"TX", (CatControl::CmdHandler)&WSJTXRadio::handleTX},              // TX
};


/*
//modified from wsjt.cpp, but WSJT-X doesn't use

// Kenwood Band
int WSJTXRadio::GetKenwoodBand() {
  int band;
  switch(t41.ActiveBand) {
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

// band down
// "BD;" (length 3) or "BDx;" (length 4)
void WSJTXRadio::handleBandDown(const char* cmd, const size_t len) {
  if(len == 3) {
    ChangeBand(-1);
  } else if(len == 4) { // hybrid
    if(atoi(&cmd[2]) == 0) {
      snprintf(msg, sizeof(msg), "BU0%d;", GetKenwoodBand());
    } else {
      snprintf(msg, sizeof(msg), "BU1%d;", GetKenwoodBand());
    }
    send(msg);
  }
}

// band up
// "BU;" (length 3) or "BUx;" (length 4)
void WSJTXRadio::handleBandUp(const char* cmd, const size_t len) {
  if(len == 3) {
    ChangeBand(1);
  } else if(len == 4) {
    if(atoi(&cmd[2]) == 0) {
      snprintf(msg, sizeof(msg), "BD0%d;", GetKenwoodBand());
    } else {
      snprintf(msg, sizeof(msg), "BD1%d;", GetKenwoodBand());
    }
    send(msg);
  }
}

*/
