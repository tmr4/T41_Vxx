// vPS specific hardware file

/*********************************************************************************************
 * modified from:
 *
 * G0ORX Front Panel
 *
 * (c) John Melton, G0ORX, 20 August 2022
 *
 * This software is made available under the GNU GPL v3 license agreement.
 *
 */

/*
 * The Project System has a single MCP23017 16 bit I/O port expander.
 * It is controlled through the I2C bus with I2C address 0x24.
 *
 * Project System info: https://protosupplies.com/product/project-system-for-teensy-4-1/
 * Project System MCP23017 schematic: https://protosupplies.com/wp-content/uploads/2023/12/Project-System-IO-Expansion-Schematic.png
 *
 * Currently a single encoder is connected.
 *
 * An interrupt on pin 40 is generated when an I/O port input changes state.
 *
 */

#include "hardwareConfig.h"

#ifdef PROJECTSYSTEM_ENCODER_MCP

#include "..\SDT.h"

#include <Adafruit_MCP23X17.h>

#include "..\Encoders.h"
#include "..\Utility.h"

#include "FrontPanel.h"

#include "..\debug.h"

//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

I2C bit_results;

int ButtonPressed = -1;
int my_ptt=HIGH;  // active LOW

#define DEBOUNCE_DELAY 250

#ifdef PROJECTSYSTEM_ENCODER_MCP
extern Rotary_V12 volumeEncoder;
#define e1 volumeEncoder
#else
extern Rotary_V12 volumeEncoder2;
#define e1 volumeEncoder2
#endif

extern Rotary_V12 menuChangeEncoder;
extern Rotary_V12 tuneEncoder;
extern Rotary_V12 fineTuneEncoder;

#define e2 menuChangeEncoder
#define e3 tuneEncoder
#define e4 fineTuneEncoder

int button_press_ms;

enum {
  PRESSED,
  RELEASED
};

//static Adafruit_MCP23X17 mcp1;
static Adafruit_MCP23X17 mcp2;

//-------------------------------------------------------------------------------------------------------------
// Forwards
//-------------------------------------------------------------------------------------------------------------

void EncoderVolume();
void EncoderFineTune();
void EncoderFilter();

//-------------------------------------------------------------------------------------------------------------
// Code
//-------------------------------------------------------------------------------------------------------------

//FASTRUN void PTT_Interrupt() {
//  my_ptt = digitalRead(PTT);
//}
/*
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
*/

FASTRUN void Mcp2Isr() {
  uint8_t pin;
  uint8_t state = 0x00;
  uint8_t a_state;
  uint8_t b_state;

  //__disable_irq();
  pin = mcp2.getLastInterruptPin();
  a_state = mcp2.readGPIOA();
  b_state = mcp2.readGPIOB();

  //Serial.print(pin); Serial.print(", "); Serial.print(a_state); Serial.print(", "); Serial.println(b_state);

  // Adafruit_MCP23X17: port A: pins 0-7, port B: pins 8-15
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
  // Port A pin 0-7: CNT1 pins 29-36, Port B pin 8-15: CNT1 pins 21-28
  // volume encoder: A-port B pin 0, CNT1 pin 21, B-port B pin 1, CNT1 pin 22, switch-port A pin 0, CNT1 pin 29
  // filter encoder: A-port B pin , CNT1 pin 23, B-port B pin , CNT1 pin 24, switch-port A pin , CNT1 pin
  // tune encoder:   A-port B pin , CNT1 pin 25, B-port B pin , CNT1 pin 26, switch-port A pin , CNT1 pin
  // fine t encoder: A-port B pin , CNT1 pin 27, B-port B pin , CNT1 pin 28, switch-port A pin , CNT1 pin
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
  // Set Wire1 I2C bus to 1MHz and start
  // *** I did not see an increased responsiveness to interrupt driven input
  //     with increased clock speed ***
  //Wire1.setClock(1000000UL);
  //Wire1.setClock(400000UL);
  Wire1.begin();

  bit_results.FRONT_PANEL_I2C_1_present = false;

  if(mcp2.begin_I2C(PROJECTSYSTEM_MCP23017_ADDR,&Wire1)) {
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
  //Serial.println(bit_results.FRONT_PANEL_I2C_2_present);
}

void PollFrontPanel() {
/*
  if(digitalRead(INT_PIN_1) == LOW) {
    //Serial.println("polling mcp1");
    Mcp1Isr();
  }
*/
  TOGGLEPROFILEPIN(PROFILER_FT8DECODE_PIN);

  if(digitalRead(INT_PIN_2) == LOW) {
    //Serial.println("polling mcp2");
    Mcp2Isr();
  }
}

#endif
