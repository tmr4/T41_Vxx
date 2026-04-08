// v12 specific hardware header file

//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

//------------
// Display.h

#define LIGHT_BLUE tft.Color565(64, 64, 192)

// radio specific display calibration factors
#define FREQSPEC_OFFSET_10DB  25

//------------
// Encoders.h
//------------
// Menu.cpp

#define MENU_RF_OPTIONS  "Power level", "Gain", "Atten In", "Atten Out", "Cancel"
#define MENU_CAL_OPTIONS "Freq Cal", "Rec Cal", "Xmit Cal", "Two Tone", "CW PA Cal", "SSB PA Cal", "Cancel"
#define MENU_RF_COUNT    5
#define MENU_CAL_COUNT   7

//------------
// Process.cpp

#define AUDIO_SPEC_SHIFT     60.0
#define AUDIO_SPEC_SHIFT_NFM 90.0
#define VOL_FACTOR            0.1
#define AUDIO_SCALER_NFM      0.025

//------------
// SDT.h

#define XMIT_SSB                  1
#define XMIT_CW                   0

//---- Teensy 4.1 Pin assignments
// *** Teensy pins are also defined in v12 FrontPanel.h
#define PTT          37    // TX input
#define RXTX         22    // TX/RX relay

// v12 RF board signals
#define RF_CW_SIGNAL 33     // CW on/off (H = on, L = off)
#define RF_XMIT_RELAY 34    // Transmit relay (H = SSB, L = CW)
#define RF_CAL_RELAY 38     // calibration relay, signal routed to board (H = input, L = output)

#define KEYER_DAH_INPUT_RING        35    // Ring connection for keyer  -- default for righthanded user
#define KEYER_DIT_INPUT_TIP         36    // Tip connection for keyer

// Pins 0 and 1 are usually reserved for the USB COM port communications
// On the Teensy 4.1 board, pins GND, 0-12, and pins 13-23, 3.3V, GND, and
// Vin are "covered up" by the Audio board. However, not all of those pins are
// actually used by the board. See: https://www.pjrc.com/store/teensy3_audio.html
// Filter Board pins
// *** these are available on the v12 main board for use with v11 front panel, but not used otherwise ***
#define FILTERPIN80M 30    // 80M filter relay
#define FILTERPIN40M 31    // 40M filter relay
#define FILTERPIN20M 28    // 20M filter relay
#define FILTERPIN15M 29    // 15M filter relay

// *** TODO: set proper profiler pins if used ***
#define PROFILER_MAINLOOP_PIN         1
#define PROFILER_PROCESS_PIN          1
#define PROFILER_DRAWFREQSPEC_PIN     1
#define PROFILER_DRAWAUDIOSPEC_PIN    1
#define PROFILER_FT8PROCESSBLOCK_PIN  1
#define PROFILER_FT8GETDATA_PIN       1
#define PROFILER_FT8DECODE_PIN        1
#define PROFILER_FT8_TX_PIN           1

//------------
// T41_SDR.ino

// hardware/band specific signal strength adjustment
// these were set with AD3 signal generator at S9 (1mW signal attenuated -73dB)
// TODO: signal strength varies by demod mode as noted; consider refinement ***
#define GAIN_CORRECTION_80M -3.0
#define GAIN_CORRECTION_40M -5.0 // gives -74 on LSB, -72 USB
#define GAIN_CORRECTION_20M -3.5
#define GAIN_CORRECTION_17M -4.5
#define GAIN_CORRECTION_15M -3.0
#define GAIN_CORRECTION_12M -1.0
#define GAIN_CORRECTION_10M -1.0

//----------
// Utility.h

extern float32_t sinBuffer4[];
extern float32_t sinBuffer5[];

extern const byte ShutdownInPin;
extern const byte ShutdownOutPin;

// Define a structure to hold the results of built-in-test routine
typedef struct {
  bool RF_I2C_present;
  bool RF_Si5351_present;
  bool BPF_I2C_present;
  bool V12_LPF_I2C_present;
  bool V12_LPF_AD7991_present;
  bool FRONT_PANEL_I2C_1_present;
  bool FRONT_PANEL_I2C_2_present;
  byte AD7991_I2C_ADDR;
} I2C;

extern I2C bit_results;

//-------------------------------------------------------------------------------------------------------------
// Code
//-------------------------------------------------------------------------------------------------------------

//------------
// Display.h

void ShowAnalogGain();

//----------
// Utility.h

void GenTwoToneBuffer(int numCycles, int tone);

void ShutDownRoutine();

void I2C_display();
