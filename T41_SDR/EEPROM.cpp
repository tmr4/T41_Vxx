
#include <EEPROM.h>
#include <SD.h>

#include "SDT.h"

#include "Button.h"
#include "Display.h"
#include "EEPROM.h"
#include "Encoders.h"
#include "Filter.h"
#include "Menu.h"
#include "Tune.h"
#include "Utility.h"

#include "debug.h"

//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

//-------------------------------------------------------------------------------------------------------------
// Forwards
//-------------------------------------------------------------------------------------------------------------

//-------------------------------------------------------------------------------------------------------------
// Code
//-------------------------------------------------------------------------------------------------------------

// *** TODO: writes to EEPROM disabled for testing purposes ***

/*****
  Save the configuration data (working variables) to EEPROM.
  Skip 4 bytes to allow for the struct size variable.
*****/
FLASHMEM void EEPROMWrite() {
  //EEPROM.put(EEPROM_BASE_ADDRESS + sizeof(int), EEPROMData);
}

/*****
  Purpose: This is nothing more than an alias for EEPROM.get(EEPROM_BASE_ADDRESS +  sizeof(int), EEPROMData).
*****/
FLASHMEM void EEPROMRead() {
  //EEPROM.get(EEPROM_BASE_ADDRESS + sizeof(int), EEPROMData);
}

/*****
  WWrite the struct size stored to the EEPROM
*****/
FLASHMEM void EEPROMWriteSize(int structSize) {
  //EEPROM.put(EEPROM_BASE_ADDRESS, structSize);
}

/*****
  Read the struct size stored in the EEPROM
*****/
FLASHMEM int EEPROMReadSize() {
  int structSize;
  EEPROM.get(EEPROM_BASE_ADDRESS, structSize);
  return structSize;
}

/*****
  Print current EEPROM values
*****/
FLASHMEM void EEPROMShow() {}

/*****
  Set EEPROM variables to default values
*****/
FLASHMEM void EEPROMSaveDefaults() {}

/*****
  Set EEPROM variables from SD card

  Return value: 0 unsuccessful, 1 ok
*****/
FLASHMEM int CopySDToEEPROM() { return 1; }

/*****
  Copy EEPROM variables to SD card

  Return value: 0 unsuccessful, 1 ok
*****/
FLASHMEM int CopyEEPROMToSD() { return 1; }

/*****
  Print SD EEPROM data
*****/
FLASHMEM void SDEEPROMDump() {}

/*****
  Manage EEPROM memory at radio start-up.
*****/
FLASHMEM void EEPROMStartup() {
/*
  int eepromStructSize;
  int stackStructSize;
  //  Determine if the struct EEPROMData is compatible (same size) with the one stored in EEPROM.

  eepromStructSize = EEPROMReadSize();
  stackStructSize = sizeof(EEPROMData);

  // For minor revisions to the code, we don't want to overwrite the EEPROM.
  // We will assume the switch matrix and other items are calibrated by the user, and not to be lost.
  // However, if the EEPROMData struct changes, it is necessary to overwrite the EEPROM with the new struct.
  // This decision is made by using a simple size comparison.  This is not fool-proof, but it will probably
  // work most of the time.  The users should be instructed to always save the EEPROM to SD for later recovery
  // of their calibration and custom settings.
  // If all else fails, then the user should execute a FLASH erase.

  // The case where struct sizes are the same, indicating no changes to the struct.  Nothing more to do, return.
  if(eepromStructSize == stackStructSize) {
    EEPROMRead();  // Read the EEPROM data into active memory.
    return;        // Done, begin radio operation.
  }

  // If the flow proceeds here, it is time to initialize some things.
  // The rest of the code will require a switch matrix calibration, and will write the EEPROMData struct to EEPROM.

//  SaveAnalogSwitchValues();         // Calibrate the switch matrix.
  //EEPROMWriteSize(stackStructSize); // Write the size of the struct to EEPROM.

  EEPROMWrite();  // Write the EEPROMData struct to non-volatile memory.

#ifdef DEBUG_EEPROM
  SDEEPROMDump();  // Call this to observe EEPROM struct data
#endif
*/
}

// *** this should be a member function of T41Property ***
void LoadOpVarsFromEEPROM(bool load /* = false */) {
  if(load) {
  }
}
