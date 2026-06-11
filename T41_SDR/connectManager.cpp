
#include "SDT.h"

#include "connectManager.h"

//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

// these don't work in DMAMEM
// not sure what these old notes refer to, but perhaps that was a heartbeat problem?

// *** force Host into DMAMEM, but not necessary??? ***
//DMAMEM USBHost USBManager::usbHost;
USBHost USBManager::usbHost;

// *** same for the Hub ***
// *** this one is key, otherwise this is created in RAM1 and we get
//     periodic delays as the hub tries to resolve the device connections ***
//DMAMEM USBHub USBManager::usbHub(USBManager::usbHost);
USBHub USBManager::usbHub(USBManager::usbHost);

//-------------------------------------------------------------------------------------------------------------
// Code
//-------------------------------------------------------------------------------------------------------------
