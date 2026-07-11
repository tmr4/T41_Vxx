// modified from: V12 BPF Board MCP23017 control (added by KI3P)

#include <Adafruit_MCP23X17.h>
#include "AD7991.h"

#include "SDT.h"

//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

static Adafruit_MCP23X17 mcpBPF; // connected to Wire2
static uint16_t BPF_GPAB_state;

// BPF band control word definitions
#define BPF_BAND_BYPASS 0x0008
#define BPF_BAND_6M     0x0004
#define BPF_BAND_10M    0x0002
#define BPF_BAND_12M    0x0001
#define BPF_BAND_15M    0x8000
#define BPF_BAND_17M    0x4000
#define BPF_BAND_20M    0x2000
#define BPF_BAND_30M    0x1000
#define BPF_BAND_40M    0x0800
#define BPF_BAND_60M    0x0100
#define BPF_BAND_80M    0x0400
//#define BPF_BAND_160M   0x0200

//-------------------------------------------------------------------------------------------------------------
// Code
//-------------------------------------------------------------------------------------------------------------

void BPFControlInit() {
  /**********************************************************************
   * Set up the BPF which is connected via the BANDS connector on Wire2 *
   **********************************************************************/
  if (mcpBPF.begin_I2C(BPF_MCP23017_ADDR,&Wire2)){
    bit_results.BPF_I2C_present = true;
    Debug("Initialising BPF board");
    mcpBPF.enableAddrPins();
    // Set all pins to be outputs
    for (int i=0;i<16;i++){
      mcpBPF.pinMode(i, OUTPUT);
    }
    // Set to BYPASS for startup.
    BPF_GPAB_state = BPF_BAND_BYPASS;
    mcpBPF.writeGPIOAB(BPF_GPAB_state);
  } else {
    bit_results.BPF_I2C_present = false;
    Debug("BPF MCP23017 not found at 0x"+String(BPF_MCP23017_ADDR,HEX));
    //ShowMessageOnWaterfall("BPF MCP23017 not found at 0x"+String(BPF_MCP23017_ADDR,HEX));
  }
}

void printBPFState(){
  Debug("BPF GPAB state: "+String(BPF_GPAB_state,BIN));
}

void setBPFBand(int currentBand) {
  switch (currentBand){

    case BAND_80M:
      BPF_GPAB_state = BPF_BAND_80M;
      break;
     case BAND_60M:
      BPF_GPAB_state = BPF_BAND_60M;
      break;
    case BAND_40M:
      BPF_GPAB_state = BPF_BAND_40M;
      break;
    case BAND_30M:
      BPF_GPAB_state = BPF_BAND_30M;
      break;
    case BAND_20M:
      BPF_GPAB_state = BPF_BAND_20M;
      break;
    case BAND_17M:
      BPF_GPAB_state = BPF_BAND_17M;
      break;
    case BAND_15M:
      BPF_GPAB_state = BPF_BAND_15M;
      break;
    case BAND_12M:
      BPF_GPAB_state = BPF_BAND_12M;
      break;
    case BAND_10M:
      BPF_GPAB_state = BPF_BAND_10M;
      break;
    case BAND_6M:
      BPF_GPAB_state = BPF_BAND_6M;
      break;
    default:
      BPF_GPAB_state = BPF_BAND_BYPASS;
      break;
  }
  mcpBPF.writeGPIOAB(BPF_GPAB_state);
  Debug("Set BPF state: "+String(BPF_GPAB_state,HEX));

}
