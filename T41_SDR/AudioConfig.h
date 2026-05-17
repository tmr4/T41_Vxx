#pragma once

#include <Audio.h>

//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

//#define MAX_AUDIO_BLOCKS 500
//#define MAX_AUDIO_BLOCKS 250
//#define MAX_AUDIO_BLOCKS 150
#define MAX_AUDIO_BLOCKS 100

extern AudioRecordQueue Q_in_L;
extern AudioRecordQueue Q_in_R;
extern AudioRecordQueue Q_in_L_Ex;
extern AudioRecordQueue Q_in_R_Ex;

extern AudioPlayQueue Q_out_L;
extern AudioPlayQueue Q_out_R;
extern AudioPlayQueue Q_out_L_Ex;
extern AudioPlayQueue Q_out_R_Ex;

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
