// v11 specific hardware header file

//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

extern uint16_t GPAB_state;

//---- Teensy 4.1 Pin assignments
// All Teensy pin assignments are made here.  This makes it easier
// when working up pin assignments for new hardware. The pins are
// divided into input and output to facilitate assignment on
// development boards for testing where some pins may be assigned
// for other purposes and some hardware may not be present.
// Conflicts with normal T41 pin assignments can cause operational
// problems.

// v11 free Teensy w/ Audio board pins:
// (Teensy sides as main board installed)
// up: 33,34,40,41
// down: 0,1,24-27

// free v11 pins after assignments below:
// up: 33,34,40,41
// down: 0,26,27

// tmp BPF board assignments: 24: SCL2, 25: SDA2

// *** Input Pins ***

#define VOLUME_ENCODER_A         2
#define VOLUME_ENCODER_B         3
#define FILTER_ENCODER_A        16
#define FILTER_ENCODER_B        15
#define FINETUNE_ENCODER_A       4
#define FINETUNE_ENCODER_B       5
#define TUNE_ENCODER_A          14
#define TUNE_ENCODER_B          17

#define PTT                         37    // TX input
#define KEYER_DAH_INPUT_RING        35    // Ring connection for keyer  -- default for righthanded user
#define KEYER_DIT_INPUT_TIP         36    // Tip connection for keyer
#define BUSY_ANALOG_PIN             39    // This is the analog pin that controls the 18 switches

// *** Output Pins ***

#define RXTX                        22    // TX/RX relay
#define MUTE                        38    // Mute Audio,  HIGH = "On" Audio available from Audio PA, LOW = Mute audio

// the v11 T41 uses an RA8875 display
#define BACKLIGHT_PIN               6
#define TFT_DC                      9
#define TFT_CS                      10
#define TFT_MOSI                    11
#define TFT_MISO                    12
#define TFT_SCLK                    13
#define TFT_RST                     255

// Filter Board pins
#define FILTERPIN80M 30    // 80M filter relay
#define FILTERPIN40M 31    // 40M filter relay
#define FILTERPIN20M 28    // 20M filter relay
#define FILTERPIN15M 29    // 15M filter relay

// *** TODO: set proper profiler pins if used ***
#define PROFILER_MAINLOOP            1
#define PROFILER_PROCESS_RX          1
#define PROFILER_DRAW                1
#define PROFILER_ENTRY               1
#define PROFILER_PROCESS_FRAME       1
#define PROFILER_RX_TX               1
#define PROFILER_DECODE_FT8          1
#define PROFILER_OTHER               1

//---- end of Teensy 4.1 Pin assignments

// v11 using v12 BPF
#define BPF_BOARD_MCP23017_ADDR 0x20   // For BPF #0 Address

// Define BPF Band words
// Word definition: GPB7 GPB6 ... GPB0 GPA7 GPA6 ... GPA0
#define BPF_BAND_BYPASS 0x0008
#define BPF_BAND_40M    0x0800

//------------
// Display.h

// radio specific display calibration factors
#define FREQSPEC_OFFSET_10DB  80

//------------
// Menu.cpp

#define MENU_RF_OPTIONS  "Power level", "Gain", "Cancel"
//#define MENU_CAL_OPTIONS "Freq Cal", "CW PA Cal", "Rec Cal", "Xmit Cal", "SSB PA Cal", "Cancel"
#define MENU_CAL_OPTIONS "Frequency", "CW Pwr", "SSB PA", "IQ", "Two Tone", "Cancel"
#define MENU_RF_COUNT    3
#define MENU_CAL_COUNT   6

//------------
// Process.cpp

// radio specific display calibration factors
#define AUDIO_SPEC_SHIFT     105.0
#define AUDIO_SPEC_SHIFT_NFM 105.0
#define VOL_FACTOR            20.0
#define AUDIO_SCALER_NFM       0.005

//------------
// T41_SDR.ino

// hardware/band specific signal strength adjustment
// these were set with AD3 signal generator at S9 (1mW signal attenuated -73dB)
// TODO: signal strength varies by demod mode as noted; consider refinement ***
#define GAIN_CORRECTION_80M -4.0
#define GAIN_CORRECTION_40M  1.0  // gives -71 on USB
#define GAIN_CORRECTION_20M -3.0
#define GAIN_CORRECTION_17M -3.0
#define GAIN_CORRECTION_15M -1.0
#define GAIN_CORRECTION_12M -1.0
#define GAIN_CORRECTION_10M -1.0

//------------
// Calibrate.h
extern int calNFAdjust;

//-------------------------------------------------------------------------------------------------------------
// Code
//-------------------------------------------------------------------------------------------------------------

//------------
// Calibrate.h
void CalibrateIQ();

void SetupBPF();

void InitFrontPanel();

inline void PollFrontPanel() {}
