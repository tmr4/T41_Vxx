
#include "SDT.h"

//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

char myGrid[] = "CM87";

const char map1[] /* PROGMEM */ = "sf.bmp";
const char map2[] /* PROGMEM */ = "beacon.bmp";
const char map3[] /* PROGMEM */ = "map.bmp";
const char map4[] /* PROGMEM */ = "map2.bmp";

const maps myMapFiles[4] /* PROGMEM */ = {
  //{ "Cincinnati.bmp", 39.07466, -84.42677 },  // Map name and coordinates for QTH
  //{ "Denver.bmp", 39.61331, -105.01664 },
  //{ "Honolulu.bmp", 21.31165, -157.89291 },
  //{ "SiestaKey.bmp", 27.26657, -82.54197 },
  { map1, 37.5, -123.0 },
  { map2, 37.5, -123.0 },
  { map3, 0.0, 0.0 },
  { map4, 0.0, 0.0 },
  //{ "", 0.0, 0.0 },
  //{ "", 0.0, 0.0 },
  //{ "", 0.0, 0.0 },
  //{ "", 0.0, 0.0 },
  //{ "", 0.0, 0.0 },
  //{ "", 0.0, 0.0 }
};

/*
w/ PROGMEM
  FLASH: code:269224, data:91232, headers:9204   free for files:7756804
   RAM1: variables:172672, code:226312, padding:3064   free for local variables:122240
   RAM2: variables:280160  free for malloc/new:244128
 EXTRAM: variables:1200320

w/o PROGMEM
  FLASH: code:269224, data:91144, headers:8268   free for files:7757828
   RAM1: variables:172672, code:226312, padding:3064   free for local variables:122240
   RAM2: variables:280160  free for malloc/new:244128
 EXTRAM: variables:1200320

So compiler is already placing these in FLASH with just the const keywords

*/
