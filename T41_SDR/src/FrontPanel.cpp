/*********************************************************************************************
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

#include "..\Encoders.h"
#include "..\Utility.h"

//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

int ButtonPressed = -1;
int my_ptt=HIGH;  // active LOW

#define DEBOUNCE_DELAY 250

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
// Code
//-------------------------------------------------------------------------------------------------------------

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
