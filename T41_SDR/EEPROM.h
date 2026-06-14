#pragma once

//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

#define NUMBER_OF_SWITCHES       18 // Number of push button switches

#define EEPROM_BASE_ADDRESS      0U

//-------------------------------------------------------------------------------------------------------------
// Code
//-------------------------------------------------------------------------------------------------------------

void EEPROMWrite();
void EEPROMRead();
void EEPROMShow();
void EEPROMSaveDefaults();
int CopySDToEEPROM();
int CopyEEPROMToSD();
void SDEEPROMDump();
void EEPROMStartup();

void LoadOpVarsFromEEPROM(bool load = false);
