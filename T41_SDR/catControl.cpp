

#include "SDT.h"

#include "radio.h"

//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

RemoteRadio radio;

//-------------------------------------------------------------------------------------------------------------
// Code
//-------------------------------------------------------------------------------------------------------------

void SendCommand(int id) {
  radio.notifyRemote(id);
}

// CAT command prefix for T41 display items and properties
// *** simplifies notifying remote unit of property changes ***
// *** list needed to be in order of id for radio elements in SDT.h
// *** as those labels are used to identify the item ***
const uint16_t CatControl::catItems[T41_ITEMS] {
  "VO"_cat,   // T41_ITEM_VOL          0
  "GT"_cat,   // T41_ITEM_AGC          1
  "F0"_cat,   // T41_ITEM_TUNE         2
  "F1"_cat,   // T41_ITEM_FINE         3
  "ZM"_cat,   // T41_ITEM_ZOOM         4
  "NG"_cat,   // T41_ITEM_FLOOR        5
  "xx"_cat,   // T41_ITEM_NOTCH        6
  "N1"_cat,   // T41_ITEM_FILTER       7
  "xx"_cat,   // T41_ITEM_COMPRESS     8
  "PG"_cat,   // T41_ITEM_RFGAIN       9
  "xx"_cat,   // T41_ITEM_EQUALIZER    10
  "xx"_cat,   // T41_ITEM_DECODER      11
  "xx"_cat,   // T41_ITEM_KEY          12
  "xx"_cat,   // T41_ITEM_KEYER        13
  "xx"_cat,   // T41_ITEM_FT8          14
  "xx"_cat,   // T41_ITEM_FT8_INT      15
  "xx"_cat,   // T41_ITEM_FT8_TX       16
  "xx"_cat,   // T41_ITEM_FT8_CQ       17
  "xx"_cat,   // T41_ITEM_FT8_TXF      18
  "xx"_cat,   // T41_ITEM_FT8_RXF      19
  "xx"_cat,   // T41_ITEM_STACK        20
  "xx"_cat,   // T41_ITEM_HEAP         21
  "xx"_cat,   // T41_ITEM_TEMP         22
  "xx"_cat,   // T41_ITEM_LOAD         23
  "FS"_cat,   // T41_ITEM_MOUSE        24 // MouseCenterTuneActive
  "NF"_cat,   // T41_ITEM_NOISE        25 // NoiseFloor
  "ME"_cat,   // T41_ITEM_RADIO_MODE   26
  "MD"_cat,   // T41_ITEM_DEMOD_MODE   27
  "BD"_cat,   // T41_ITEM_BAND         28 // ActiveBand
  "PC"_cat,   // T41_ITEM_POWER        29 // TxPower
  "FC"_cat,   // T41_ITEM_FREQ         30 // CenterFreq
  "FF"_cat,   // T41_ITEM_NCO          31 // NCOFreq
  "NH"_cat,   // T41_ITEM_FHI          32 // FilterHiCut
  "NL"_cat,   // T41_ITEM_FLO          33 // FilterLoCut
  "xx"_cat,   // T41_ITEM_SCALE        34 // FreqSpecScale
  "xx"_cat,   // T41_ITEM_CW_FILTER    35 // CWFilterIndex
  "xx"_cat    // T41_ITEM_CW_DECODER   36
};

//void CatControl::send(const char *msg) {
//  link->print(msg);
//  //ethernetControl.flush();
//}
