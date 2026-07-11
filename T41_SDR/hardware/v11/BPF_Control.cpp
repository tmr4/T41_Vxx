// modified from: V12 BPF Board MCP23017 control (added by KI3P)

#include "..\SDT.h"

#ifdef USE_BPF_BOARD

#include <Wire.h>
#include <Adafruit_MCP23X17.h>

//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

Adafruit_MCP23X17 mcpBPF; // connected to Wire2
uint16_t BPF_GPAB_state;

#define BPF_BOARD_MCP23017_ADDR 0x20   // For BPF #0 Address

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

FLASHMEM void SetupBPF() {
  // Set Wire2 I2C bus to 100KHz and start
  Wire2.setClock(100000UL);
  Wire2.begin();

  while (!mcpBPF.begin_I2C(BPF_BOARD_MCP23017_ADDR,&Wire2)){
    Serial.println("BPF MCP23017 not found at 0x"+String(BPF_BOARD_MCP23017_ADDR,HEX));
    delay(5000);
  }

  //Serial.println("BPF connected");

  // Enable the address pins A0, A1, and A2.
  mcpBPF.enableAddrPins();
  // Set all chip pins to be outputs
  for (int i=0;i<16;i++){
    mcpBPF.pinMode(i, OUTPUT);
  }

  // Set to 40m band
  BPF_GPAB_state = BPF_BAND_40M;
  //BPF_GPAB_state = BPF_BAND_BYPASS;
  mcpBPF.writeGPIOAB(BPF_GPAB_state);
}

FLASHMEM void SetBPFBand(int currentBand) {
  switch (currentBand) {
    /*
    case BAND_80M:
      BPF_GPAB_state = BPF_BAND_80M;
      break;
    //case BAND_60M:
    //  BPF_GPAB_state = BPF_BAND_60M;
    //  break;
    */
    case BAND_40M:
      BPF_GPAB_state = BPF_BAND_40M;
      //Serial.println("BPF 40m");
      break;
    /*
    //case BAND_30M:
    //  BPF_GPAB_state = BPF_BAND_30M;
    //  break;
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
    //case BAND_6M:
    //  BPF_GPAB_state = BPF_BAND_6M;
    //  break;
    */
    default:
      BPF_GPAB_state = BPF_BAND_BYPASS;
      //Serial.println("BPF bypassed");
      break;
  }
  mcpBPF.writeGPIOAB(BPF_GPAB_state);
}

#endif
