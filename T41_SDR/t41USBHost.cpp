
#include <USBHost_t36.h> // https://github.com/PaulStoffregen/USBHost_t36

#include "SDT.h"

#include "keyboard.h"
#include "mouse.h"
//#include "t41Control.h"
#include "t41USBHost.h"

//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

/*  it would be nice to save this memory until a keyboard is plugged in
    but both USBHost and USBHIDParser are needed to automatically detect
    a new devise so we don't really save that much.  Doing this manually
    is a possibility if we need to save memory when not using a keyboard. */
#if USB_HOST_SUPPORT
USBHost usbHost;
USBHub usbHub(usbHost);
#endif

#if HOST_KEYBOARD_MOUSE_SUPPORT
USBHIDParser hkbParser(usbHost); // each device needs a parser
KeyboardController kbController(usbHost);

USBHIDParser mouseParser(usbHost); // each device needs a parser
MouseController mouseController(usbHost);
#endif

//-------------------------------------------------------------------------------------------------------------
// Code
//-------------------------------------------------------------------------------------------------------------

FLASHMEM void UsbHostSetup() {
#if HOST_KEYBOARD_MOUSE_SUPPORT
  KeyboardSetup();
#endif
}

void UsbHostLoop() {
#if USB_HOST_SUPPORT
  usbHost.Task();
#endif

#if HOST_KEYBOARD_MOUSE_SUPPORT
  MouseLoop();
#endif
}
