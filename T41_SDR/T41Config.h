#pragma once

//====================== User Specific Preferences =============

//#define DEBUG 1                         // Uncommented for debugging, comment out for normal use

#ifdef DEBUG
#define DEBUG_MESSAGES
#endif

#ifdef DEBUG_MESSAGES
#define Debug(x) Serial.println(x)
#else
#define Debug(x)
#endif

#define debugSerial Serial // Serial or SerialUSB1 or SerialUSB2

//#define DEBUG_SW // debug switch matrix false presses
//#define DEBUG_EEPROM

//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

// select display type
#define RA8875_DISPLAY      1
//#define ILI9488_DISPLAY     1
//#define NO_DISPLAY          1

#define Si_5351_crystal     25000000L

// Audio chain configuration options
//#define USE_MIC_COMPRESSION 1
//#define AUDIO_STATS         1

#define DECODER_STATE							0						    // 0 = off, 1 = on
#define DEFAULT_KEYER_WPM   			15              // Startup value for keyer wpm
#define FREQ_SEP_CHARACTER  			'.'					    // Some may prefer period, space, or combo
#define MAP_FILE_NAME   					"sf.bmp"        // Name you gave to BMP map file. Max is 50 chars
#define MY_LAT										37.5            // Coordinates for QTH actually CM87
#define MY_LON										-123.0
#define MY_CALL										"Your Call"     // Default max is 10 chars
#define MY_TIMEZONE          			"PST: "         // Default max is 10 chars
#define TIME_24H                  1               // comment for 12h display

#define PADDLE_FLIP								0						    // 0 = right paddle = DAH, 1 = DIT
#define STRAIGHT_KEY_OR_PADDLES		0						    // 0 = straight, 1 = paddles

#define CURRENT_FREQ_A            7048000         // VFO_A
#define CURRENT_FREQ_B            7030000         // VFO_B

                                                  //            0   1   2     3     4      5       6        7
#define DEFAULTFREQINDEX          6               // Default: (10, 50, 100, 250, 1000, 10000, 100000, 1000000)
                                                  //            0   1   2     3
#define DEFAULT_FT_INDEX          3               // Default: (10, 50, 250, 500)

#define DEFAULT_POWER_LEVEL       1               // Startup power level

#define SPLASH_DELAY              4000L           // How long to show Splash screen

#define STARTUP_BAND        			1               // This is the 40M band (EEPROM.h)

#define CENTER_SCREEN_X           400
#define CENTER_SCREEN_Y           245
#define IMAGE_CORNER_X            162             // ImageWidth = 378 Therefore 800 - 378 = 422 / 2 = 211
#define IMAGE_CORNER_Y            0               // ImageHeight = 302 Therefore 480 - 302 = 178 / 2 = 89
#define RAY_LENGTH                190

#define USE_FULL_MENU             0               // 0 - use top line menus; 1 - use full screen menus

#define SDCARD_MESSAGE_LENGTH     3000L  // The number of milliseconds to leave error message on screen

#define BEACON_FILE_NAME          "beacon.bmp"

#define T41_USB_AUDIO // *** for passing audio back and forth with WSJT-X over USB at 44.1kHz sample rate in FT8 mode ***

#define FT8_EXTERNAL_MEMORY
