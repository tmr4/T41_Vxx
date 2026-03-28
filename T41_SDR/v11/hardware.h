// v11 specific hardware header file

//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

extern uint16_t GPAB_state;

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
// Encoders.h

//---- Teensy 4.1 Pin assignments
// *** TODO: rework front panel stuff to eliminate this ***
#if defined(FOURSQRP_FRONTPANEL)
    #define VOLUME_ENCODER_A         2
    #define VOLUME_ENCODER_B         3
    #define FILTER_ENCODER_A        16
    #define FILTER_ENCODER_B        15
    #define FINETUNE_ENCODER_A       4
    #define FINETUNE_ENCODER_B       5
    #define TUNE_ENCODER_A          14
    #define TUNE_ENCODER_B          17
#elif defined(MCP23017_FRONTPANEL)
    #define VOLUME_ENCODER_A         2
    #define VOLUME_ENCODER_B         3
    #define FILTER_ENCODER_A        15
    #define FILTER_ENCODER_B        14
    #define FINETUNE_ENCODER_A       4
    #define FINETUNE_ENCODER_B       5
    #define TUNE_ENCODER_A          16
    #define TUNE_ENCODER_B          17
#endif

#define PTT          37    // TX input
#define RXTX         22    // TX/RX relay

#define KEYER_DAH_INPUT_RING        35    // Ring connection for keyer  -- default for righthanded user
#define KEYER_DIT_INPUT_TIP         36    // Tip connection for keyer

// Pins 0 and 1 are usually reserved for the USB COM port communications
// On the Teensy 4.1 board, pins GND, 0-12, and pins 13-23, 3.3V, GND, and
// Vin are "covered up" by the Audio board. However, not all of those pins are
// actually used by the board. See: https://www.pjrc.com/store/teensy3_audio.html
// Filter Board pins
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
// SDT.h

#define MUTE                        38    // Mute Audio,  HIGH = "On" Audio available from Audio PA, LOW = Mute audio
#define BUSY_ANALOG_PIN             39    // This is the analog pin that controls the 18 switches

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

//-------------------------------------------------------------------------------------------------------------
// Code
//-------------------------------------------------------------------------------------------------------------

void SetupBPF();

//------------
// Process.h

void RemoveDCBias();
float CalcSignalStrength();

//------------
// T41_SDR.ino

void InitHardware();
void SoftResetHardware();
void ConfigRadioStateHardware();
void HardwareLoopStart();
