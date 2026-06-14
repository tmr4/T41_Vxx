
#include "USBManager.h"

//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

// these don't work in DMAMEM
// I've left these old notes which relate to an attempt to track down a delayed start
// and other delays due to USB hub ennumeration

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
