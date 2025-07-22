
//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

//------------
// Display.h

#define LIGHT_BLUE tft.Color565(64, 64, 192)

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
#define FREQSPEC_OFFSET_10DB 25

//------------
// SDT.h

#define XMIT_SSB                  1
#define XMIT_CW                   0

#define PTT                         37    // Transmit/Receive

// v12 RF board signals
#define RF_CW_SIGNAL 33     // CW on/off (H = on, L = off)
#define RF_XMIT_RELAY 34    // Transmit relay (H = SSB, L = CW)
#define RF_CAL_RELAY 38     // calibration relay, signal routed to board (H = input, L = output)

#ifdef FOURSQRP_FRONTPANEL
#define BUSY_ANALOG_PIN             39    // This is the analog pin that controls the 18 switches
#endif

#ifdef PROJECTSYSTEM_EXPANDED_IO
#define INT_PIN_1 41
#define INT_PIN_2 40
#else
#define INT_PIN_1 14
#define INT_PIN_2 15
#endif

//------------
// T41Control.h

extern bool signalStrengthReceived;
extern float signalStrength;
extern int signalStrengthReceivedIndex;

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

//------------
// Encoders.h


//------------
// Process.h

void RemoveDCBias();
float32_t CalcSignalStrength();

//------------
// T41Control.h

void SendSetFreq(int freq);
void SendSetMode(int mode);
void SendSignalStrengthRequest();
void SendSignalStrengthRequest(int index);
void SendSetDisplayZoom(int zoom);
void SendSetNarrowFilter();
void SendSetBandChange(int upDown);

//------------
// T41_SDR.ino

void InitHardware();
void SoftResetHardware();
void ConfigRadioStateHardware();
void HardwareLoopStart();

//----------
// Utility.h

void GenTwoToneBuffer(int numCycles, int tone);

void ShutDownRoutine();

void I2C_display();
