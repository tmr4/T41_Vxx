// vPS specific Front Panel hardware file

//#include <stdint.h>
//#include "Rotary_V12.h"

//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

//#define FRONT_PANEL_POLLING_OPS

//-------------------------------------------------------------------------------------------------------------
// Code
//-------------------------------------------------------------------------------------------------------------

void InitFrontPanel();

#ifdef FRONT_PANEL_POLLING_OPS
inline void PollFrontPanel() {
  if(digitalRead(INT_PIN_1) == LOW) {
    Mcp1Isr();
  }
  if(digitalRead(INT_PIN_2) == LOW) {
    Mcp2Isr();
  }
}
#else
inline void PollFrontPanel() {}
#endif
