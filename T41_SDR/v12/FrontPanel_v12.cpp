// v12 specific hardware file

/*********************************************************************************************
 * some code modified from:
 *
 * G0ORX Front Panel
 *
 * (c) John Melton, G0ORX, 20 August 2022
 *
 * This software is made available under the GNU GPL v3 license agreement.
 *
 */

/*
 * The front panel consists of 2 MCP23017 16 bit I/O port expanders.
 * Each device is controlled through the I2C bus and the devices use the I2C address 0x20 and 0x21.
 *
 * The device at 0x20 has switches 1..16 connected to it.
 *
 * The device at 0x21 has the switches 17 and 18, encoder 1..4 switches, encoder 1..4 A and B and 2 output LEDS.
 *
 * An interrupt is generated when an I/O port input changes state.
 *
 * The device at 0x20 generates an interrupt on pin 14 (pulls it low) - Pin 6 of the IDC connector.
 * The device at 0x21 generates an interrupt on pin 15 (pulls it low) - Pin 8 of the IDC connector.
 *
 * The IDC connector is connected to Tune/Filter IDC connector of the Main Board.
 *
 */

#include "..\SDT.h"

#include <Adafruit_MCP23X17.h>
#include <stdint.h>

#include "..\Button.h"
#include "..\CWProcessing.h"
#include "..\Encoders.h"
#include "FrontPanel.h"
#include "..\MenuProc.h"
#include "..\Tune.h"
#include "..\Utility.h"

#include "Rotary_V12.h"

//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

#define VOLUME_REVERSED false
#define FILTER_REVERSED false
#define MAIN_TUNE_REVERSED false
#define FINE_TUNE_REVERSED false

//#define AUDIO_VOLUME 0
//#define MIC_GAIN 1
//#define AGC_GAIN 2
//#define SIDETONE_VOLUME 3
//#define NOISE_FLOOR_LEVEL 4
//#define SQUELCH_LEVEL 5

int ButtonPressed = -1;
int my_ptt=HIGH;  // active LOW

#define DEBOUNCE_DELAY 250

Rotary_V12 volumeEncoder( VOLUME_REVERSED );
Rotary_V12 tuneEncoder( MAIN_TUNE_REVERSED );
Rotary_V12 menuChangeEncoder( FILTER_REVERSED );
Rotary_V12 fineTuneEncoder( FINE_TUNE_REVERSED );

#define e1 volumeEncoder
#define e2 menuChangeEncoder
#define e3 tuneEncoder
#define e4 fineTuneEncoder

int button_press_ms;

enum {
  PRESSED,
  RELEASED
};

static Adafruit_MCP23X17 mcp1;
static Adafruit_MCP23X17 mcp2;

#define LED1 0
#define LED2 1

#define LED_1_PORT 6
#define LED_2_PORT 7

//-------------------------------------------------------------------------------------------------------------
// Forwards
//-------------------------------------------------------------------------------------------------------------

void EncoderVolume();
void EncoderFineTune();
void EncoderFilter();

//-------------------------------------------------------------------------------------------------------------
// Code
//-------------------------------------------------------------------------------------------------------------

// Switch Matrix

/*****
  Purpose: Determine which UI button was pressed

  Parameter list:
    int valPin            the ADC value from analogRead()

  Return value:
    int                   -1 if not valid push button, index of push button if valid
*****/
int ProcessButtonPress(int valPin) {
  return valPin;
}

/*****
  Purpose: Check for UI button press. If pressed, return the ADC value

  Parameter list:
    none

  Return value:
    int                   -1 if not valid push button, ADC value if valid
*****/
int ReadSelectedPushButton() {
  int pressed;

  //__disable_irq();

  PollFrontPanel();

  pressed = ButtonPressed;
  ButtonPressed = BOGUS_PIN_READ;
  //__enable_irq();

  return pressed;
}

// Encoders

/*****
  Purpose: Encoder volume control
*****/
// why not FASTRUN
// TODO: front panel placeholders for now
void EncoderVolume() {
  int result;

  result = volumeEncoder.process();  // Read the encoder


  if(result == 0) {  // Nothing read
    return;
  }

  adjustVolEncoder = result;

  if((calibrateItem >= 1) && (calibrateItem <= 3)) return;

  audioVolume += adjustVolEncoder;
  adjustVolEncoder = 0;

  if(audioVolume > MAX_AUDIO_VOLUME) {
    audioVolume = MAX_AUDIO_VOLUME;
  } else if(audioVolume < MIN_AUDIO_VOLUME) {
    audioVolume = MIN_AUDIO_VOLUME;
  }

  volumeChangeFlag = true; // flag needed for display update
}

/*****
  Purpose: Fine tune control
*****/
// TODO: front panel placeholders for now
void EncoderFineTune() {
  int result;

  result = fineTuneEncoder.process();  // Read the encoder

// *** TODO: we'll go through here many times if fine tune encoder bounces ***
  // *** If fineTuneEncoderMove isn't processed in the meantime,
  //    and result == 0, then fineTuneEncoderMove will be reset to zero ***

  if(result == 0) {                   // Nothing read
    fineTuneEncoderMove = 0L;
    return;
  }

  fineTuneEncoderMove = result;

  // *** TODO: from v12, validate v11 calibration routines
  // fine tune used in calibration routines, return to process
  //   - receive calibrate adjusts In/Out attenuation
  //   - transmit calibrate adjusts In/Out attenuation
  //   - two tone adjusts tone 2
  if((calibrateItem >= 1) && (calibrateItem <= 3)) {
    // TODO: not currently used in v12
    //calNFAdjust -= fineTuneEncoderMove;
    fineTuneEncoderMove = 0;
    return;
  }

  SetFineTune(ftIncrement * fineTuneEncoderMove);

  fineTuneEncoderMove = 0L;
}

/*****
  Purpose: Menu/Change/Filter encoder movement
*****/
// TODO: front panel placeholders for now
void EncoderFilter() {
  int result;

  result = menuChangeEncoder.process();  // Read the encoder

  if(result == 0) {
    return;
  }

  menuEncoderMove = result;

  ProcessMenuEncoder();
}

// MCP23017

//FASTRUN void PTT_Interrupt() {
//  my_ptt = digitalRead(PTT);
//}

FASTRUN void Mcp1Isr() {
  uint8_t pin;
  uint8_t state;

  //__disable_irq();
  while((pin = mcp1.getLastInterruptPin())!=MCP23XXX_INT_ERR) {
    state = mcp1.digitalRead(pin);
    if(state == PRESSED) {
      if((millis()-button_press_ms)>DEBOUNCE_DELAY){
        ButtonPressed = pin;
        button_press_ms = millis();
      }
    } else {
      //buttonReleased(pin1);
    }
  }
  //__enable_irq();
}

FASTRUN void Mcp2Isr() {
  uint8_t pin;
  uint8_t state = 0x00;
  uint8_t a_state;
  uint8_t b_state;

  //__disable_irq();
  pin = mcp2.getLastInterruptPin();
  a_state = mcp2.readGPIOA();
  b_state = mcp2.readGPIOB();
  switch(pin) {
    case 8:
    case 9:
      state = b_state & 0x03;
      break;
    case 10:
    case 11:
      state = (b_state >> 2) & 0x03;
      break;
    case 12:
    case 13:
      state = (b_state >> 4) & 0x03;
      break;
    case 14:
    case 15:
      state = (b_state >> 6) & 0x03;
      break;
  }

  // process the state
  switch(pin) {
    case 8:
      e1.updateA(state);
      EncoderVolume();
      break;
    case 9:
      e1.updateB(state);
      EncoderVolume();
      break;
    case 10:
      e2.updateA(state);
      EncoderFilter();
      break;
    case 11:
      e2.updateB(state);
      EncoderFilter();
      break;
    case 12:
      e3.updateA(state);
      break;
    case 13:
      e3.updateB(state);
      break;
    case 14:
      e4.updateA(state);
      EncoderFineTune();
      break;
    case 15:
      e4.updateB(state);
      EncoderFineTune();
      break;
    case 0:
    case 1:
    case 2:
    case 3:
    case 4:
    case 5:
      state = (a_state >> pin) & 0x01;
      if(state == PRESSED) {
        if((millis()-button_press_ms)>DEBOUNCE_DELAY){
          ButtonPressed = (pin+16);
          button_press_ms = millis();
        }
      } else {
        //buttonReleased(pin2+16);
      }

      break;
    default:
      // 255 sometimes caused by switch bounce
      //Debug(String(__FUNCTION__)+": "+String(pin)+"!");
      break;
  }
  //__enable_irq();
}

void InitFrontPanel() {
  Debug("Initializing G0ORX front panel");

  // Set Wire1 I2C bus to 1MHz and start
  //Wire1.setClock(1000000UL);
  Wire1.begin();

  if(mcp1.begin_I2C(V12_PANEL_MCP23017_ADDR_1,&Wire1)) {
    bit_results.FRONT_PANEL_I2C_1_present = true;

    // setup the device
    mcp1.setupInterrupts(true, true, LOW);

    // setup switches 1..16
    for(int i = 0; i < 16; i++) {
      mcp1.pinMode(i, INPUT_PULLUP);
      mcp1.setupInterruptPin(i, CHANGE);
    }

    // clear interrupts
    mcp1.readGPIOAB(); // ignore any return value

    pinMode(INT_PIN_1, INPUT_PULLUP);
#ifndef FRONT_PANEL_POLLING_OPS
    attachInterrupt(digitalPinToInterrupt(INT_PIN_1), Mcp1Isr, LOW);
#endif
  } else {
    //ShowMessageOnWaterfall("MCP23017 not found at 0x"+String(G0ORX_PANEL_MCP23017_ADDR_1,HEX));
    bit_results.FRONT_PANEL_I2C_1_present = false;
  }

  if(mcp2.begin_I2C(V12_PANEL_MCP23017_ADDR_2,&Wire1)) {
    bit_results.FRONT_PANEL_I2C_2_present = true;

    // setup the device
    mcp2.setupInterrupts(true, true, LOW);

    // setup switches 17..18 and Encoder switches 1..4 (note 6 and 7 are output LEDs)
    for(int i = 0; i < 6; i++) {
      mcp2.pinMode(i, INPUT_PULLUP);
      mcp2.setupInterruptPin(i, CHANGE);
    }

    //mcp2.pinMode(LED_1_PORT, OUTPUT);  // LED1
    //mcp2.digitalWrite(LED_1_PORT, LOW);
    //mcp2.pinMode(LED_2_PORT, OUTPUT);  // LED2
    //mcp2.digitalWrite(LED_2_PORT, LOW);

    // setup encoders 1..4 A and B
    for(int i = 8; i < 16; i++) {
      mcp2.pinMode(i, INPUT_PULLUP);
      mcp2.setupInterruptPin(i, CHANGE);
    }

    mcp2.readGPIOAB(); // ignore any return value

    pinMode(INT_PIN_2, INPUT_PULLUP);
#ifndef FRONT_PANEL_POLLING_OPS
    attachInterrupt(digitalPinToInterrupt(INT_PIN_2), Mcp2Isr, LOW);
#endif
  } else {
    //ShowMessageOnWaterfall("MCP23017 not found at 0x"+String(G0ORX_PANEL_MCP23017_ADDR_2,HEX));
    bit_results.FRONT_PANEL_I2C_2_present = false;
  }
}

//FASTRUN void FrontPanelSetLed(int led, uint8_t state) {
//  switch(led) {
//    case LED1:
//      mcp2.digitalWrite(LED_1_PORT, state);
//      break;
//    case LED2:
//      mcp2.digitalWrite(LED_2_PORT, state);
//      break;
//  }
//}

int ReadTuneEncoder() {
  return tuneEncoder.process();
}
