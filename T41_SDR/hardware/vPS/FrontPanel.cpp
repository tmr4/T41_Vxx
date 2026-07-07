// vPS specific Front Panel hardware file

#include "..\SDT.h"

#include "..\Button.h"
#include "..\Encoders.h"

#if !defined(PROJECTSYSTEM_ENCODER_MCP)

// v11 type encoders and switches
#include <Bounce.h>

#include "..\CWProcessing.h"
#include "..\Menu.h"
#include "..\MenuProc.h"
#include "..\Tune.h"
#include "..\Utility.h"

//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

extern int calibrateItem;

#define NUMBER_OF_SWITCHES       18 // Number of push button switches

#ifdef PROJECTSYSTEM_SWITCH_MATRIX
int bandswitchPins[] = {
  FILTERPIN80M,  // 80M
  FILTERPIN40M,  // 40M
  FILTERPIN20M,  // 20M
  FILTERPIN15M,  // 17M
  FILTERPIN15M,  // 15M
  0,   // 12M  Note that 12M and 10M both use the 10M filter, which is always in (no relay).  KF5N September 27, 2023.
  0    // 10M
};
#endif
#ifdef PROJECTSYSTEM_VOLUME_ENCODER
Rotary volumeEncoder = Rotary(VOLUME_ENCODER_A, VOLUME_ENCODER_B);        // ( 2,  3)
Bounce encoderSwitch = Bounce(VOLUME_SWITCH, 10);  // 10 ms debounce
#endif
#ifdef PROJECTSYSTEM_FILTER_ENCODER
Rotary menuChangeEncoder = Rotary(FILTER_ENCODER_A, FILTER_ENCODER_B);        // ( 2,  3)
Bounce encoder2Switch = Bounce(FILTER_SWITCH, 10);  // 10 ms debounce
#endif
#ifdef PROJECTSYSTEM_FINETUNE_ENCODER
Rotary fineTuneEncoder = Rotary(FINETUNE_ENCODER_A, FINETUNE_ENCODER_B);  // ( 4,  5)
Bounce encoder3Switch = Bounce(FINETUNE_SWITCH, 10);  // 10 ms debounce
#endif
#ifdef PROJECTSYSTEM_TUNE_ENCODER
Rotary tuneEncoder = Rotary(TUNE_ENCODER_A, TUNE_ENCODER_B);              // (16, 17)
Bounce encoder4Switch = Bounce(TUNE_SWITCH, 10);  // 10 ms debounce
#endif

int buttonRead = 0;
int minPinRead = 1024;

/*
The button interrupt routine implements a first-order recursive filter, or "leaky integrator,"
as described at:

  https://www.edn.com/a-simple-software-lowpass-filter-suits-embedded-system-applications/

Filter bandwidth is dependent on the sample rate and the "k" parameter, as follows:

                                1 Hz
                          k   Bandwidth   Rise time (samples)
                          1   0.1197      3
                          2   0.0466      8
                          3   0.0217      16
                          4   0.0104      34
                          5   0.0051      69
                          6   0.0026      140
                          7   0.0012      280
                          8   0.0007      561

Thus, the default values below create a filter with 10000 * 0.0217 = 217 Hz bandwidth
*/

#define BUTTON_FILTER_SAMPLERATE 10000  // Hz
#define BUTTON_FILTER_SHIFT 3           // Filter parameter k
#define BUTTON_DEBOUNCE_DELAY 5000      // uSec

#define BUTTON_STATE_UP 0
#define BUTTON_STATE_DEBOUNCE 1
#define BUTTON_STATE_PRESSED 2

#define BUTTON_USEC_PER_ISR (1000000 / BUTTON_FILTER_SAMPLERATE)

#define BUTTON_OUTPUT_UP 1023  // Value to be output when in the UP state

#define NOTHING_TO_SEE_HERE         950     // If the analog pin is greater than this value, nothing's going on

IntervalTimer buttonInterrupts;
bool buttonInterruptsEnabled = false;
static unsigned long buttonFilterRegister;
static int buttonState, buttonADCPressed, buttonElapsed;
static volatile int buttonADCOut;

int buttonThresholdPressed = 944;   // switchValues[0] + WIGGLE_ROOM
int buttonThresholdReleased = 964;  // buttonThresholdPressed + WIGGLE_ROOM
int buttonRepeatDelay = 300000;     // Increased to 300000 from 200000 to better handle cheap, wornout buttons.

int switchValues[NUMBER_OF_SWITCHES] = { 922, 871, 818, 768, 716, 669, 612, 566, 515, 462, 406, 355, 300, 241, 187, 127, 67, 5 };

//-------------------------------------------------------------------------------------------------------------
// Forwards
//-------------------------------------------------------------------------------------------------------------

void EncoderVolumeISR();

//-------------------------------------------------------------------------------------------------------------
// Code
//-------------------------------------------------------------------------------------------------------------

// set up encoders
//#ifdef PROJECTSYSTEM_TUNE_ENCODER
#if defined(PROJECTSYSTEM_VOLUME_ENCODER) || defined(PROJECTSYSTEM_FILTER_ENCODER) || defined(PROJECTSYSTEM_FINETUNE_ENCODER) || defined(PROJECTSYSTEM_FILTER_ENCODER)
void EncodersInit() {
#ifdef PROJECTSYSTEM_VOLUME_ENCODER
  volumeEncoder.begin(true);
  attachInterrupt(digitalPinToInterrupt(VOLUME_ENCODER_A), EncoderVolumeISR, CHANGE);
  attachInterrupt(digitalPinToInterrupt(VOLUME_ENCODER_B), EncoderVolumeISR, CHANGE);

  // set up encoder switch debounce
  pinMode(VOLUME_SWITCH, INPUT_PULLUP);
#endif
#ifdef PROJECTSYSTEM_FILTER_ENCODER
  pinMode(FILTER_ENCODER_A, INPUT);
  pinMode(FILTER_ENCODER_B, INPUT);

  menuChangeEncoder.begin(true);
  attachInterrupt(digitalPinToInterrupt(FILTER_ENCODER_A), EncoderMenuChangeFilterISR, CHANGE);
  attachInterrupt(digitalPinToInterrupt(FILTER_ENCODER_B), EncoderMenuChangeFilterISR, CHANGE);

  // set up encoder switch debounce
  pinMode(FILTER_SWITCH, INPUT_PULLUP);
#endif
#ifdef PROJECTSYSTEM_FINETUNE_ENCODER
  fineTuneEncoder.begin(true);
  attachInterrupt(digitalPinToInterrupt(FINETUNE_ENCODER_A), EncoderFineTuneISR, CHANGE);
  attachInterrupt(digitalPinToInterrupt(FINETUNE_ENCODER_B), EncoderFineTuneISR, CHANGE);
#endif
#ifdef PROJECTSYSTEM_TUNE_ENCODER
  tuneEncoder.begin(true);
  attachInterrupt(digitalPinToInterrupt(TUNE_ENCODER_A), EncoderCenterTuneISR, CHANGE);
  attachInterrupt(digitalPinToInterrupt(TUNE_ENCODER_B), EncoderCenterTuneISR, CHANGE);
#endif
}
#endif

#ifdef PROJECTSYSTEM_VOLUME_ENCODER
/*****
  Purpose: Encoder volume control ISR
*****/
// why not FASTRUN
void EncoderVolumeISR() {
  char result = 0;

  result = volumeEncoder.process();  // Read the encoder

  if(result == 0) {  // Nothing read
    return;
  }

  // TODO: check encoder setup as this is opposite T41
  switch(result) {
    case DIR_CW:  // Turned it clockwise, 16
      adjustVolEncoder = -1;
      break;

    case DIR_CCW:  // Turned it counter-clockwise
      adjustVolEncoder = 1;
      break;
  }

  if((calibrateItem >= 1) && (calibrateItem <= 3)) return;

  t41.AudioVolume += adjustVolEncoder;
  adjustVolEncoder = 0;
}
#endif

#ifdef PROJECTSYSTEM_FILTER_ENCODER
/*****
  Purpose: Menu/Change/Filter encoder movement ISR
*****/
FASTRUN void EncoderMenuChangeFilterISR() {
  char result;

  result = menuChangeEncoder.process();  // Read the encoder

  if(result == 0) {
    return;
  }

  switch(result) {
    case DIR_CW:  // Turned it clockwise, 16
      menuEncoderMove = 1;
      break;

    case DIR_CCW:  // Turned it counter-clockwise
      menuEncoderMove = -1;
      break;
  }

  ProcessMenuEncoder();
}
#endif

#ifdef PROJECTSYSTEM_FINETUNE_ENCODER
/*****
  Purpose: Fine tune control ISR
*****/
FASTRUN void EncoderFineTuneISR() {
  char result;

  result = fineTuneEncoder.process();  // Read the encoder

// *** TODO: we'll go through here many times if fine tune encoder bounces ***
  // *** If fineTuneEncoderMove isn't processed in the meantime,
  //    and result == 0, then fineTuneEncoderMove will be reset to zero ***

  if(result == 0) {                   // Nothing read
    fineTuneEncoderMove = 0L;
    return;
  }

  switch(result) {
    case DIR_CW:  // Turned it clockwise, 16
      fineTuneEncoderMove = 1;
      break;

    case DIR_CCW:  // Turned it counter-clockwise
      fineTuneEncoderMove = -1;
      break;
  }

  // *** TODO: from v12, validate v11 calibration routines
  // fine tune used in calibration routines, return to process
  //   - receive calibrate adjusts In/Out attenuation
  //   - transmit calibrate adjusts In/Out attenuation
  //   - two tone adjusts tone 2
  if((calibrateItem >= 1) && (calibrateItem <= 3)) {
    // -= fineTuneEncoderMove;
    fineTuneEncoderMove = 0;
    return;
  }

  t41.NCOFreq += t41.FtIncrement() * fineTuneEncoderMove;

  fineTuneEncoderMove = 0L;
}
#endif

#ifdef PROJECTSYSTEM_TUNE_ENCODER
/*****
  Project System was missing center tune pulses with the polling setup
  Capturing encoder movement with an interrupt and polling a global value
  fixed that.
*****/

/*****
  Purpose: handle center tune interrupt
  sets tuneChange to be handled as radio proccesses controls
  this makes tune change happen from known location
*****/
void EncoderCenterTuneISR() {
  unsigned char result = tuneEncoder.process();

  if(result == 0)
    return;

  switch(result) {
    case DIR_CW:  // Turned it clockwise, 16
      tuneChange = 1;
      break;

    case DIR_CCW:  // Turned it counter-clockwise
      tuneChange = -1;
      break;
  }
}

int ReadTuneEncoder() { return 0; }

#endif

#ifdef PROJECTSYSTEM_SWITCH_MATRIX
// T41 Switch Labels
const char *labels[] = { "Select", "Menu Up", "Band Up",
                         "Zoom", "Menu Dn", "Band Dn",
                         "Filter", "DeMod", "Mode",
                         "NR", "Notch", "Noise Floor",
                         "Fine Tune", "Decoder", "Tune Increment",
                         "Reset Tuning", "Frequ Entry", "User 2" };

/*****
  Purpose: ISR to read button ADC and detect button presses

  Parameter list:
    none*****/
void ButtonISR() {
  int filteredADCValue;

  buttonFilterRegister = buttonFilterRegister - (buttonFilterRegister >> BUTTON_FILTER_SHIFT) + analogRead(BUSY_ANALOG_PIN);
  filteredADCValue = (int)(buttonFilterRegister >> BUTTON_FILTER_SHIFT);

  switch(buttonState) {
    case BUTTON_STATE_UP:
      if(filteredADCValue <= buttonThresholdPressed) {
        buttonElapsed = 0;
        buttonState = BUTTON_STATE_DEBOUNCE;
      }
      break;

    case BUTTON_STATE_DEBOUNCE:
      if(buttonElapsed < BUTTON_DEBOUNCE_DELAY) {
        buttonElapsed += BUTTON_USEC_PER_ISR;
      } else {
        buttonADCOut = buttonADCPressed = filteredADCValue;
        buttonElapsed = 0;
        buttonState = BUTTON_STATE_PRESSED;
      }
      break;

    case BUTTON_STATE_PRESSED:
      if(filteredADCValue >= buttonThresholdReleased) {
        buttonState = BUTTON_STATE_UP;
        } else if(buttonRepeatDelay != 0) {  // buttonRepeatDelay of 0 disables repeat
          if(buttonElapsed < buttonRepeatDelay) {
            buttonElapsed += BUTTON_USEC_PER_ISR;
          } else {
            buttonADCOut = buttonADCPressed;
            buttonElapsed = 0;
          }
        }
      break;
  }
}

/*****
  Purpose: Starts button IntervalTimer and toggles subsequent button
           functions into interrupt mode.

  Parameter list:
    none*****/
FLASHMEM void EnableButtonInterrupts() {
  buttonADCOut = BUTTON_OUTPUT_UP;
  buttonFilterRegister = buttonADCOut << BUTTON_FILTER_SHIFT;
  buttonState = BUTTON_STATE_UP;
  buttonADCPressed = BUTTON_STATE_UP;
  buttonElapsed = 0;
  buttonInterrupts.begin(ButtonISR, 1000000 / BUTTON_FILTER_SAMPLERATE);
  buttonInterruptsEnabled = true;
}

/*****
  Purpose: Determine which UI button was pressed

  Parameter list:
    int valPin            the ADC value from analogRead()

  Return value:
    int                   -1 if not valid push button, index of push button if valid
*****/
int ProcessButtonPress(int valPin) {
  int switchIndex;

  if(valPin == BOGUS_PIN_READ) {  // Not valid press
#ifdef DEBUG_SW
  Serial.println("NAM BOGUS_PIN_READ");
#endif
    return -1;
  }

  if(valPin == MENU_OPTION_SELECT && menuStatus == NO_MENUS_ACTIVE) {
#ifdef DEBUG_SW
  Serial.println("NAM #2");
#endif
    return -1;
  }

  for(switchIndex = 0; switchIndex < NUMBER_OF_SWITCHES; switchIndex++) {
    if(abs(valPin - switchValues[switchIndex]) < WIGGLE_ROOM)  // ...because ADC does return exact values every time
    {
      return switchIndex;
    }
  }

  return -1;  // Really should never do this
}

/*****
  Purpose: Check for UI button press. If pressed, return the ADC value

  Parameter list:
    none

  Return value:
    int                   -1 if not valid push button, ADC value if valid
*****/
int ReadSelectedPushButton() {
  minPinRead = 0;
  int buttonReadOld = 1023;

  if(buttonInterruptsEnabled) {
    noInterrupts();
    buttonRead = buttonADCOut;

    /*
    Clear the button read.  If the button remains pressed, the ISR will reset the value nearly
    instantly.  Clearing the value here rather than in the ISR provides more consistent button
    press "feel" when calls to ReadSelectedPushButton have variable timing.
    */

    buttonADCOut = BUTTON_OUTPUT_UP;
    interrupts();
  } else {
    while(abs(minPinRead - buttonReadOld) > 3) {  // do averaging to smooth out the button response
      minPinRead = analogRead(BUSY_ANALOG_PIN);

      buttonRead = .1 * minPinRead + (1 - .1) * buttonReadOld;  // See expected values in next function.
      buttonReadOld = buttonRead;
    }
  }

  if(buttonRead > switchValues[0] + WIGGLE_ROOM) {
    return -1;
  }
  minPinRead = buttonRead;
  if(!buttonInterruptsEnabled) {
    delay(100L);
  }

  return minPinRead;
}

/*****
  Purpose: function reads the analog value for each matrix switch
*****/
FLASHMEM void SaveAnalogSwitchValues() {
  int index;
  int minVal;
  int value;
  int origRepeatDelay;
/*
  tft.clearMemory();  // Need to clear overlay too
  tft.writeTo(L2);
  tft.fillWindow();
  tft.writeTo(L1);
  tft.clearScreen(RA8875_BLACK);
  tft.setFontScale(1);
  tft.setTextColor(RA8875_GREEN);
  tft.setCursor(10, 10);
  tft.print("Press button you");
  tft.setCursor(10, 30);
  tft.print("have assigned to");
  tft.setCursor(10, 50);
  tft.print("the switch shown.");
*/
  Serial.println("Press button you have assigned to the switch shown:");

  // Disable button repeat for interrupt driven buttons
  origRepeatDelay = buttonRepeatDelay;
  buttonRepeatDelay = 0;

  for(index = 0; index < NUMBER_OF_SWITCHES;) {
    /*
    tft.setCursor(20, 100);
    tft.print(index + 1);
    tft.print(". ");
    tft.print(labels[index]);
    */
    Serial.print(index + 1); Serial.print(". "); Serial.print(labels[index]); Serial.print(" ");
    if(buttonInterruptsEnabled) {
      while((value = ReadSelectedPushButton()) == -1) {
        // Wait until a button is pressed
      }
    } else {
      value = -1;
      minVal = NOTHING_TO_SEE_HERE;
      while(true) {
        value = ReadSelectedPushButton();
        if(value < NOTHING_TO_SEE_HERE && value > 0) {
          delay(100L);
          if(value < minVal) {
            minVal = value;
          } else {
            value = minVal;
            break;
          }
        }
      }
    }

    /*
    tft.fillRect(20, 100, 300, 40, RA8875_BLACK);
    tft.setCursor(350, 20 + index * 25);
    tft.print(index + 1);
    tft.print(". ");
    tft.print(labels[index]);
    tft.setCursor(660, 20 + index * 25);
    tft.print(value);
    */
    Serial.println(value);
    switchValues[index] = value;

    // Set interrupt press/release thresholds based on the Select button, which has the highest ADC value
    if(index == 0) {
      buttonThresholdPressed = switchValues[0] + WIGGLE_ROOM;
      buttonThresholdReleased = buttonThresholdPressed + WIGGLE_ROOM;
    }

    index++;
    while((value = ReadSelectedPushButton()) != -1 && value < NOTHING_TO_SEE_HERE) {
      // Wait until the button is released
    }
  }

  buttonRepeatDelay = origRepeatDelay;  // Restore original repeat delay
}

void PreChangeBandHardware() {
  if(t41.ActiveBand < BAND_12M) {
    digitalWrite(bandswitchPins[t41.ActiveBand], LOW);
  }
}
void PostChangeBandHardware() {
  if(t41.ActiveBand < BAND_12M) {
    digitalWrite(bandswitchPins[t41.ActiveBand], HIGH);
  }
}
#else
// *** TODO: need empty functions back for hardwareless Project System ***
#endif

#else

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

#include <Adafruit_MCP23X17.h>

#include "..\Button.h"
#include "..\Utility.h"

#include "..\debug.h"

//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

// Define a structure to hold the results of built-in-test routine
typedef struct {
  bool RF_I2C_present;
  bool RF_Si5351_present;
  bool BPF_I2C_present;
  bool V12_LPF_I2C_present;
  bool V12_LPF_AD7991_present;
  bool FRONT_PANEL_I2C_1_present;
  bool FRONT_PANEL_I2C_2_present;
  byte AD7991_I2C_ADDR;
} I2C;

I2C bit_results;

int ButtonPressed = -1;
int my_ptt=HIGH;  // active LOW

#define DEBOUNCE_DELAY 250

Rotary_V12 volumeEncoder( VOLUME_REVERSED );
Rotary_V12 tuneEncoder( MAIN_TUNE_REVERSED );
Rotary_V12 menuChangeEncoder( FILTER_REVERSED );
Rotary_V12 fineTuneEncoder( FINE_TUNE_REVERSED );

#define e1 volumeEncoder2
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

#ifdef FRONT_PANEL_POLLING_OPS
  PollFrontPanel();
#endif

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

  t41.AudioVolume += adjustVolEncoder;
  adjustVolEncoder = 0;
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

  t41.NCOFreq += t41.FtIncrement() * fineTuneEncoderMove;

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
#endif
