#pragma once

#include <Audio.h>

#include "input_tcp.h"
#include "output_tcp.h"
#include "input_udp.h"
#include "output_udp.h"
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

#if RADIO_ROLE == 1
extern AudioOutputHostSerial iqStreamUSB;
extern AudioOutputUDP iqStreamUDP;
#elif RADIO_ROLE == 2
extern AudioInputSerial1 iqStreamUSB;
extern AudioInputUDP iqStreamUDP;
#endif
extern AudioConnectBase* iqStream;

//-------------------------------------------------------------------------------------------------------------
// Code
//-------------------------------------------------------------------------------------------------------------

void AudioSetup(int sampleRate, bool supportsTX = true);
void ConfigAudioState(int audioState);

void SetupRemoteIQStream(int connectMode);

void SetupMicCompressors(boolean use_HP_filter, float knee_dBFS, float comp_ratio, float attack_sec, float release_sec);

#ifdef AUDIO_STATS
void StartAudioStats();
void EndAudioStats();
#endif

inline void __attribute__((always_inline)) YieldToEthernet() {
#if RADIO_ROLE == 1
  iqStream->writeToQueue();
#elif RADIO_ROLE == 2
  iqStream->readToQueue();
#endif
}
