// include after SDT.h to get proper configureation defines
// *** TODO: consider restructuring ***

//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

#define USB_HOST_SUPPORT false
#if HOST_KEYBOARD_MOUSE_SUPPORT
  #undef USB_HOST_SUPPORT
  #define USB_HOST_SUPPORT true
#endif

//-------------------------------------------------------------------------------------------------------------
// Code
//-------------------------------------------------------------------------------------------------------------

void UsbHostSetup();
void UsbHostLoop();
