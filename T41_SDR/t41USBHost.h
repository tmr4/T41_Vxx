// include after SDT.h to get proper configureation defines
// *** TODO: consider restructuring ***

//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

#define USB_HOST_SUPPORT false
#if HOST_KEYBOARD_MOUSE_SUPPORT || CAT_CONTROL_T41_USB_HOST || SEND_IQ_TO_REMOTE_USB
  #undef USB_HOST_SUPPORT
  #define USB_HOST_SUPPORT true
#endif

#if CAT_CONTROL_T41_USB_HOST || SEND_IQ_TO_REMOTE_USB
#define HOST_SERIAL_SUPPORT true // uses USBSerial_BigBuffer
#endif

//-------------------------------------------------------------------------------------------------------------
// Code
//-------------------------------------------------------------------------------------------------------------

void UsbHostSetup();
void UsbHostLoop();
