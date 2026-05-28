
#include "SDT.h"

#include "connectManager.h"

//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

// *** force Host into DMAMEM, but not necessary??? ***
DMAMEM USBHost USBManager::usbHost;

// *** same for the Hub ***
// *** this one is key, otherwise this is created in RAM1 and we get
//     periodic delays as the hub tries to resolve the device connections ***
DMAMEM USBHub USBManager::usbHub(USBManager::usbHost);

//-------------------------------------------------------------------------------------------------------------
// Code
//-------------------------------------------------------------------------------------------------------------
