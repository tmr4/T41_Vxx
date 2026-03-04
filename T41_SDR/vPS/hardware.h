#pragma once

// vPS project system specific hardware header file

#include "hardwareConfig.h"

//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

//------------
// Display.h

// radio specific display calibration factors
#define FREQSPEC_OFFSET_10DB  80

//------------
// Encoders.h

//---- Teensy 4.1 Pin assignments
#if defined(FOURSQRP_FRONTPANEL)
    #define VOLUME_ENCODER_A         2
    #define VOLUME_ENCODER_B         3
    #define FILTER_ENCODER_A        16
    #define FILTER_ENCODER_B        15
    #define FINETUNE_ENCODER_A       4
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
#elif defined(PROJECTSYSTEM_EXPANDED_IO)
#else
    #ifdef PROJECTSYSTEM_FINETUNE_ENCODER
    #define FINETUNE_ENCODER_A       4
    #define FINETUNE_ENCODER_B      24 // pin 5 is TFT_CS on Project System (the pin assigned here is only meaningful when testing fine tune encoder on non-front panel systems)
    #endif
    #ifdef PROJECTSYSTEM_ENCODER_1
    #define VOLUME_ENCODER_A         4
    #define VOLUME_ENCODER_B         3
    #define ENCODER_1_SWITCH         2
    #endif

    #define PROFILER_MAINLOOP_PIN         33
    #define PROFILER_PROCESS_PIN          34
    #define PROFILER_DRAWFREQSPEC_PIN     41
    #define PROFILER_DRAWAUDIOSPEC_PIN    14
    #define PROFILER_FT8PROCESSBLOCK_PIN  35
    #define PROFILER_FT8GETDATA_PIN       36
    #define PROFILER_FT8DECODE_PIN        38
#endif

#define PTT          37    // Transmit/Receive

#ifdef PROJECTSYSTEM_ENCODER_1
#include <Rotary.h>                    // https://github.com/brianlow/Rotary

extern Rotary volumeEncoder;        // (2,  3)
#endif

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
#define BUSY_ANALOG_PIN             40    // pin 39 is TFT_MISO on Project System (the pin assigned here is only meaningful when testing switch matrix on non-front panel systems)

#ifdef PROJECTSYSTEM_EXPANDED_IO_40
#define INT_PIN_2 40
#endif
#ifdef PROJECTSYSTEM_EXPANDED_IO_41
#define INT_PIN_1 41
#endif

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

//------------
// Encoders.h

#ifdef PROJECTSYSTEM_ENCODER_1
void EncodersInit();
void EncoderVolumeISR();
#endif

#ifdef PROJECTSYSTEM_ENCODER_MCP
#endif
