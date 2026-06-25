#pragma once

#include <Audio.h>

#include "audioInOutQueue.h"
#include "ethernetQueue.h"
#include "input_usb.h"
#include "output_usb.h"

//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

                                // heap left
//#define MAX_AUDIO_BLOCKS 500  // 93k
#define MAX_AUDIO_BLOCKS 250    // 156k
//#define MAX_AUDIO_BLOCKS 150
//#define MAX_AUDIO_BLOCKS 100

extern AudioRecordQueue Q_in_L;
extern AudioRecordQueue Q_in_R;
extern AudioRecordQueue Q_in_L_Ex;
extern AudioRecordQueue Q_in_R_Ex;

extern AudioPlayQueue Q_out_L;
extern AudioPlayQueue Q_out_R;
extern AudioPlayQueue Q_out_L_Ex;
extern AudioPlayQueue Q_out_R_Ex;

#if USB_ENABLED
#if RADIO_ROLE == 0
extern AudioOutputHostSerial iqStreamUSB;
#elif RADIO_ROLE == 1
extern AudioInputSerial1 iqStreamUSB;
#endif
#endif

#if ETHERNET_ENABLED
extern AudioInputFromQueue iqStreamIn;
extern AudioOutputToQueue iqStreamOut;
extern EthernetQueue iqQueue;
#endif

//-------------------------------------------------------------------------------------------------------------
// Code
//-------------------------------------------------------------------------------------------------------------

void AudioSetup(int sampleRate, bool supportsTX = true);
void ConfigAudioState(int audioState);

void SetupMicCompressors(boolean use_HP_filter, float knee_dBFS, float comp_ratio, float attack_sec, float release_sec);

#ifdef AUDIO_STATS
void StartAudioStats();
void EndAudioStats();
#endif

inline void __attribute__((always_inline)) YieldToEthernet() {
#if ETHERNET_ENABLED
#if RADIO_ROLE == 0
    iqQueue.writeFromQueue();
#elif RADIO_ROLE == 1
    iqQueue.readToQueue();
#endif
#endif
}
