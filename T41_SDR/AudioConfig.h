#pragma once

#include <Audio.h>

#include "audioInOutQueue.h"
#include "ethernetBridge.h"
//#include "input_tcp.h"
//#include "output_tcp.h"
#include "input_ethernet.h"
#include "output_ethernet.h"
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

#if RADIO_ROLE == 7
extern AudioOutputHostSerial iqStreamUSB;
extern AudioOutputEthernet iqStreamEthernet;
#elif RADIO_ROLE == 4
extern AudioInputEthernet iqStreamEthernet;
#elif RADIO_ROLE == 6
extern AudioInputSerial1 iqStreamUSB;
extern AudioInputEthernet iqStreamEthernet;
#elif RADIO_ROLE == 14 || RADIO_ROLE == 15
extern AudioInputFromQueue iqStreamIn;
extern AudioOutputToQueue iqStreamOut;
extern EthernetBridgeQueue iqQueue;
#endif
#if RADIO_ROLE == 14
extern EthernetBridgeClient iqEthernet;
#elif RADIO_ROLE == 15
extern EthernetBridgeServer iqEthernet;
#endif
extern ConnectBase* cbStream;

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
  if(cbStream) {
#if RADIO_ROLE == 7
    cbStream->writeToQueue();
#elif RADIO_ROLE == 4 || RADIO_ROLE == 6
    cbStream->readToQueue();
#endif
  }
#if RADIO_ROLE == 15
    iqQueue.writeFromQueue();
#elif RADIO_ROLE == 14
    iqQueue.readToQueue();
#endif
}
