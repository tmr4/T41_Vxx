#include "..\SDT.h"

#include "..\Button.h"

//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

extern int ButtonPressed;

//-------------------------------------------------------------------------------------------------------------
// Forwards
//-------------------------------------------------------------------------------------------------------------

//-------------------------------------------------------------------------------------------------------------
// Code
//-------------------------------------------------------------------------------------------------------------



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
  if(digitalRead(INT_PIN_1) == LOW) {
    Mcp1Isr();
  }
  if(digitalRead(INT_PIN_2) == LOW) {
    Mcp2Isr();
  }
#endif

  pressed = ButtonPressed;
  ButtonPressed = BOGUS_PIN_READ;
  //__enable_irq();

  return pressed;
}
