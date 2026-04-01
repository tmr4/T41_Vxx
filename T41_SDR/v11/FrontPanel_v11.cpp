// v11 Front Panel

#include <Rotary.h>                    // https://github.com/brianlow/Rotary

#include "..\SDT.h"

#include "..\Button.h"
#include "Calibrate.h"
#include "..\CWProcessing.h"
#include "..\Display.h"
#include "..\EEPROM.h"
#include "..\Encoders.h"
#include "..\Menu.h"
#include "..\MenuProc.h"
#include "..\Tune.h"
#include "..\Utility.h"

//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

// Encoders

//---- Teensy 4.1 Pin assignments
#define VOLUME_ENCODER_A         2
#define VOLUME_ENCODER_B         3
#define FILTER_ENCODER_A        16
#define FILTER_ENCODER_B        15
#define FINETUNE_ENCODER_A       4
#define FINETUNE_ENCODER_B       5
#define TUNE_ENCODER_A          14
#define TUNE_ENCODER_B          17

// Pin assignments
// volumeEncoder      (2,  3)
// tuneEncoder        (16, 17)
// menuChangeEncoder  (14, 15)
// fineTuneEncoder    (4,  5);
Rotary fineTuneEncoder = Rotary(FINETUNE_ENCODER_A, FINETUNE_ENCODER_B);  // ( 4,  5)
Rotary menuChangeEncoder = Rotary(FILTER_ENCODER_A, FILTER_ENCODER_B);    // (15, 14)
Rotary tuneEncoder = Rotary(TUNE_ENCODER_A, TUNE_ENCODER_B);              // (16, 17)
Rotary volumeEncoder = Rotary(VOLUME_ENCODER_A, VOLUME_ENCODER_B);        // ( 2,  3)

// Switch Matrix

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

#ifndef ALT_ISR
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
#else
#define BUTTON_FILTER_SAMPLERATE 10000  // Hz
#define BUTTON_FILTER_SHIFT 3           // Filter parameter k
//#define BUTTON_DEBOUNCE_DELAY 5000      // uSec
#define BUTTON_DEBOUNCE_DELAY 2500      // uSec
#define BUTTON_DEBOUNCE_RELEASE_DELAY 2500      // uSec

#define BUTTON_STATE_UP 0
#define BUTTON_STATE_DEBOUNCE 1
#define BUTTON_STATE_PRESSED 2
#define BUTTON_STATE_RELEASE_DEBOUNCE 3

#define BUTTON_USEC_PER_ISR (1000000 / BUTTON_FILTER_SAMPLERATE)

#define BUTTON_OUTPUT_UP 1023  // Value to be output when in the UP state

IntervalTimer buttonInterrupts;
bool buttonInterruptsEnabled = false;
static unsigned long buttonFilterRegister;
//static int buttonState, buttonADCPressed, buttonElapsed;
static int buttonADCPressed, buttonElapsed;
int buttonState = BUTTON_STATE_UP;
static volatile int buttonADCOut;
#endif

// T41 Switch Labels
const char *labels[] = { "Select", "Menu Up", "Band Up",
                         "Zoom", "Menu Dn", "Band Dn",
                         "Filter", "DeMod", "Mode",
                         "NR", "Notch", "Noise Floor",
                         "Fine Tune", "Decoder", "Tune Increment",
                         "Reset Tuning", "Frequ Entry", "User 2" };


//-------------------------------------------------------------------------------------------------------------
// Forwards
//-------------------------------------------------------------------------------------------------------------

void EncoderFineTuneISR();
void EncoderMenuChangeFilterISR();
void EncoderVolumeISR();

void ProcessMenuEncoder();

//-------------------------------------------------------------------------------------------------------------
// Code
//-------------------------------------------------------------------------------------------------------------

// Encoders

// set up encoders
void EncodersInit() {
  pinMode(FILTER_ENCODER_A, INPUT);
  pinMode(FILTER_ENCODER_B, INPUT);

  tuneEncoder.begin(true);
  volumeEncoder.begin(true);
  attachInterrupt(digitalPinToInterrupt(VOLUME_ENCODER_A), EncoderVolumeISR, CHANGE);
  attachInterrupt(digitalPinToInterrupt(VOLUME_ENCODER_B), EncoderVolumeISR, CHANGE);
  menuChangeEncoder.begin(true);
  attachInterrupt(digitalPinToInterrupt(FILTER_ENCODER_A), EncoderMenuChangeFilterISR, CHANGE);
  attachInterrupt(digitalPinToInterrupt(FILTER_ENCODER_B), EncoderMenuChangeFilterISR, CHANGE);
  fineTuneEncoder.begin(true);
  attachInterrupt(digitalPinToInterrupt(FINETUNE_ENCODER_A), EncoderFineTuneISR, CHANGE);
  attachInterrupt(digitalPinToInterrupt(FINETUNE_ENCODER_B), EncoderFineTuneISR, CHANGE);
}

/*****
  Purpose: Set center tune frequency based on
*****/
void EncoderCenterTune() {
  unsigned char result;

  result = tuneEncoder.process();  // Read the encoder

  if(result == 0)  // Nothing read
    return;

  if(radioMode == CW_MODE && decoderFlag == ON) {  // No reason to reset if we're not doing decoded CW
    ResetHistograms();
  }

  switch(result) {
    case DIR_CW:  // Turned it clockwise, 16
      tuneChange = 1;
      break;

    case DIR_CCW:  // Turned it counter-clockwise
      tuneChange = -1;
      break;
  }

  // *** TODO: from v12, validate v11 calibration routines
  // center tune used in calibration routines, return to process
  //   - receive calibrate adjusts noise floor
  //   - transmit calibrate adjusts image value
  //   - two tone adjusts tone 1
  if((calibrateItem >= 1) && (calibrateItem <= 3)) return;

  SetCenterTune((long)freqIncrement * tuneChange);
}

/*****
  Purpose: Encoder volume control ISR
*****/
// why not FASTRUN
void EncoderVolumeISR() {
  char result;

  result = volumeEncoder.process();  // Read the encoder

  if(result == 0) {  // Nothing read
    return;
  }

  switch(result) {
    case DIR_CW:  // Turned it clockwise, 16
      adjustVolEncoder = 1;
      break;

    case DIR_CCW:  // Turned it counter-clockwise
      adjustVolEncoder = -1;
      break;
  }

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
    calNFAdjust -= fineTuneEncoderMove;
    fineTuneEncoderMove = 0;
    return;
  }

  SetFineTune(ftIncrement * fineTuneEncoderMove);

  fineTuneEncoderMove = 0L;
}

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

// Switch Matrix

/*****
  Purpose: ISR to read button ADC and detect button presses

  Parameter list:
    none*****/
void ButtonISR() {
#ifndef ALT_ISR
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
#else
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
        //buttonADCPressed = filteredADCValue;
        buttonElapsed = 0;
        buttonState = BUTTON_STATE_PRESSED;
      }
      break;

    case BUTTON_STATE_PRESSED:
      if(filteredADCValue >= buttonThresholdReleased) {
        buttonState = BUTTON_STATE_UP;
        //buttonState = BUTTON_STATE_RELEASE_DEBOUNCE;
      } else {
        if(buttonRepeatDelay != 0) {  // buttonRepeatDelay of 0 disables repeat
          if(buttonElapsed < buttonRepeatDelay) {
            buttonElapsed += BUTTON_USEC_PER_ISR;
          } else {
            buttonADCOut = buttonADCPressed;
            buttonElapsed = 0;
          }
        }
      }
      break;

    case BUTTON_STATE_RELEASE_DEBOUNCE:
      if(buttonElapsed < BUTTON_DEBOUNCE_RELEASE_DELAY) {
        buttonElapsed += BUTTON_USEC_PER_ISR;
      } else {
        buttonADCOut = buttonADCPressed;
        buttonElapsed = 0;
        buttonState = BUTTON_STATE_UP;
      }
      break;
  }
#endif
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
    if(abs(valPin - EEPROMData.switchValues[switchIndex]) < WIGGLE_ROOM)  // ...because ADC does return exact values every time
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

  if(buttonRead > EEPROMData.switchValues[0] + WIGGLE_ROOM) {
    return -1;
  }
  minPinRead = buttonRead;
  if(!buttonInterruptsEnabled) {
    delay(100L);
  }

  return minPinRead;
}

/*****
  Purpose: function reads the analog value for each matrix switch and stores that value in EEPROM.
*****/
FLASHMEM void SaveAnalogSwitchValues() {
  int index;
  int minVal;
  int value;
  int origRepeatDelay;

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

  // Disable button repeat for interrupt driven buttons
  origRepeatDelay = EEPROMData.buttonRepeatDelay;
  EEPROMData.buttonRepeatDelay = 0;

  for(index = 0; index < NUMBER_OF_SWITCHES;) {
    tft.setCursor(20, 100);
    tft.print(index + 1);
    tft.print(". ");
    tft.print(labels[index]);

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

    tft.fillRect(20, 100, 300, 40, RA8875_BLACK);
    tft.setCursor(350, 20 + index * 25);
    tft.print(index + 1);
    tft.print(". ");
    tft.print(labels[index]);
    tft.setCursor(660, 20 + index * 25);
    tft.print(value);
    EEPROMData.switchValues[index] = value;

    // Set interrupt press/release thresholds based on the Select button, which has the highest ADC value
    if(index == 0) {
      EEPROMData.buttonThresholdPressed = EEPROMData.switchValues[0] + WIGGLE_ROOM;
      EEPROMData.buttonThresholdReleased = EEPROMData.buttonThresholdPressed + WIGGLE_ROOM;
    }

    index++;
    while((value = ReadSelectedPushButton()) != -1 && value < NOTHING_TO_SEE_HERE) {
      // Wait until the button is released
    }
  }

  EEPROMData.buttonRepeatDelay = origRepeatDelay;  // Restore original repeat delay
}

// General Front Panel Stuff
void InitFrontPanel() {
  EnableButtonInterrupts();
  EncodersInit();
}
