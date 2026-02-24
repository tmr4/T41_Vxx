
#include <Arduino.h>

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <stdbool.h>
#include <time.h>
#include <TimeLib.h>                   // Part of Teensy Time library

#include "constants.h"
#include "decode.h"
#include "encode.h"
#include "message.h"
#include "..\common\common.h"
#include "..\common\monitor.h"

//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

static float *signal;
static int numSamples;
static monitor_t *monitor;

const int kMin_score = 10; // Minimum sync score threshold for candidates
const int kMax_candidates = 140;
const int kLDPC_iterations = 25;

static const int kMax_decoded_messages = 50;

const int kFreq_osr = 2; // Frequency oversampling rate (bin subdivision)
const int kTime_osr = 2; // Time oversampling rate (symbol subdivision)
//const int kFreq_osr = 1; // Frequency oversampling rate (bin subdivision)
//const int kTime_osr = 1; // Time oversampling rate (symbol subdivision)

#define CALLSIGN_HASHTABLE_SIZE 256

static struct {
  char callsign[12]; ///> Up to 11 symbols of callsign + trailing zeros (always filled)
  uint32_t hash;     ///> 8 MSBs contain the age of callsign; 22 LSBs contain hash value
} callsign_hashtable[CALLSIGN_HASHTABLE_SIZE];

static int callsign_hashtable_size;

#ifndef float32_t
typedef float float32_t;
#endif

//-------------------------------------------------------------------------------------------------------------
// Forwards
//-------------------------------------------------------------------------------------------------------------

void AddDecodedMessage(struct tm *tmSlot, int16_t score, float time_sec, float freq_hz, char *msg);

void ft8_DrawSpectrum(uint8_t *spec, int numSamples);

//-------------------------------------------------------------------------------------------------------------
// Code
//-------------------------------------------------------------------------------------------------------------

void hashtable_init(void)
{
  callsign_hashtable_size = 0;
  memset(callsign_hashtable, 0, sizeof(callsign_hashtable));
}

void hashtable_cleanup(uint8_t max_age)
{
  for(int idx_hash = 0; idx_hash < CALLSIGN_HASHTABLE_SIZE; ++idx_hash) {
    if(callsign_hashtable[idx_hash].callsign[0] != '\0') {
      uint8_t age = (uint8_t)(callsign_hashtable[idx_hash].hash >> 24);
      if(age > max_age) {
        // free the hash entry
        callsign_hashtable[idx_hash].callsign[0] = '\0';
        callsign_hashtable[idx_hash].hash = 0;
        callsign_hashtable_size--;
      } else {
        // increase callsign age
        callsign_hashtable[idx_hash].hash = (((uint32_t)age + 1u) << 24) | (callsign_hashtable[idx_hash].hash & 0x3FFFFFu);
      }
    }
  }
}

void hashtable_add(const char* callsign, uint32_t hash) {
  uint16_t hash10 = (hash >> 12) & 0x3FFu;
  int idx_hash = (hash10 * 23) % CALLSIGN_HASHTABLE_SIZE;

  while(callsign_hashtable[idx_hash].callsign[0] != '\0') {
    if(((callsign_hashtable[idx_hash].hash & 0x3FFFFFu) == hash) && (0 == strcmp(callsign_hashtable[idx_hash].callsign, callsign))) {
      // reset age
      callsign_hashtable[idx_hash].hash &= 0x3FFFFFu;
      return;
    } else {
      // Move on to check the next entry in hash table
      idx_hash = (idx_hash + 1) % CALLSIGN_HASHTABLE_SIZE;
    }
  }

  callsign_hashtable_size++;
  strncpy(callsign_hashtable[idx_hash].callsign, callsign, 11);
  callsign_hashtable[idx_hash].callsign[11] = '\0';
  callsign_hashtable[idx_hash].hash = hash;
}

bool hashtable_lookup(ftx_callsign_hash_type_t hash_type, uint32_t hash, char* callsign) {
  uint8_t hash_shift = (hash_type == FTX_CALLSIGN_HASH_10_BITS) ? 12 : (hash_type == FTX_CALLSIGN_HASH_12_BITS ? 10 : 0);
  uint16_t hash10 = (hash >> (12 - hash_shift)) & 0x3FFu;
  int idx_hash = (hash10 * 23) % CALLSIGN_HASHTABLE_SIZE;

  while(callsign_hashtable[idx_hash].callsign[0] != '\0') {
    if(((callsign_hashtable[idx_hash].hash & 0x3FFFFFu) >> hash_shift) == hash) {
      strcpy(callsign, callsign_hashtable[idx_hash].callsign);
      return true;
    }

    // Move on to check the next entry in hash table
    idx_hash = (idx_hash + 1) % CALLSIGN_HASHTABLE_SIZE;
  }

  callsign[0] = '\0';
  return false;
}

ftx_callsign_hash_interface_t hash_if = {
  .lookup_hash = hashtable_lookup,
  .save_hash = hashtable_add
};

void decode(const monitor_t* mon, struct tm* tm_slot_start) {
  const ftx_waterfall_t* wf = &mon->wf;
  // Find top candidates by Costas sync score and localize them in time and frequency
  //ftx_candidate_t candidate_list[kMax_candidates];
  ftx_candidate_t *candidate_list = (ftx_candidate_t *)extmem_malloc(kMax_candidates * sizeof(ftx_candidate_t));
  int num_candidates = ftx_find_candidates(wf, kMax_candidates, candidate_list, kMin_score);

  // Hash table for decoded messages (to check for duplicates)
  int num_decoded = 0;
  //ftx_message_t decoded[kMax_decoded_messages];
  ftx_message_t *decoded = (ftx_message_t *)extmem_malloc(kMax_decoded_messages * sizeof(ftx_message_t));
  //ftx_message_t* decoded_hashtable[kMax_decoded_messages];
  ftx_message_t **decoded_hashtable = (ftx_message_t **)malloc(kMax_decoded_messages * sizeof(ftx_message_t *));

  // Initialize hash table pointers
  for(int i = 0; i < kMax_decoded_messages; ++i) {
    decoded_hashtable[i] = NULL;
  }

  // Go over candidates and attempt to decode messages
  for(int idx = 0; idx < num_candidates; ++idx) {
    const ftx_candidate_t* cand = &candidate_list[idx];

    float freq_hz = (mon->min_bin + cand->freq_offset + (float)cand->freq_sub / wf->freq_osr) / mon->symbol_period;
    float time_sec = (cand->time_offset + (float)cand->time_sub / wf->time_osr) * mon->symbol_period;

#ifdef WATERFALL_USE_PHASE
    // int resynth_len = 12000 * 16;
    // float resynth_signal[resynth_len];
    // for(int pos = 0; pos < resynth_len; ++pos)
    // {
    //     resynth_signal[pos] = 0;
    // }
    // monitor_resynth(mon, cand, resynth_signal);
    // char resynth_path[80];
    // sprintf(resynth_path, "resynth_%04f_%02.1f.wav", freq_hz, time_sec);
    // save_wav(resynth_signal, resynth_len, 12000, resynth_path);
#endif

    ftx_message_t message;
    ftx_decode_status_t status;
    if(!ftx_decode_candidate(wf, cand, kLDPC_iterations, &message, &status)) {
      continue;
    }

    int idx_hash = message.hash % kMax_decoded_messages;
    bool found_empty_slot = false;
    bool found_duplicate = false;
    do {
      if(decoded_hashtable[idx_hash] == NULL) {
        found_empty_slot = true;
      }
      else if((decoded_hashtable[idx_hash]->hash == message.hash) && (0 == memcmp(decoded_hashtable[idx_hash]->payload, message.payload, sizeof(message.payload)))) {
        found_duplicate = true;
      } else {
        // Move on to check the next entry in hash table
        idx_hash = (idx_hash + 1) % kMax_decoded_messages;
      }
    } while(!found_empty_slot && !found_duplicate);

    if(found_empty_slot) {
      // Fill the empty hashtable slot
      memcpy(&decoded[idx_hash], &message, sizeof(message));
      decoded_hashtable[idx_hash] = &decoded[idx_hash];
      ++num_decoded;

      char text[FTX_MAX_MESSAGE_LENGTH];
      ftx_message_offsets_t offsets;
      ftx_message_rc_t unpack_status = ftx_message_decode(&message, &hash_if, text, &offsets);
      if(unpack_status != FTX_MESSAGE_RC_OK) {
        snprintf(text, sizeof(text), "Error [%d] while unpacking!", (int)unpack_status);
      }

      // Fake WSJT-X-like output for now
      //float snr = cand->score * 0.5f; // TODO: compute better approximation of SNR
      //printf("%02d%02d%02d %+05.1f %+4.2f %4.0f ~  %s\n",
      //    tm_slot_start->tm_hour, tm_slot_start->tm_min, tm_slot_start->tm_sec,
      //    snr, time_sec, freq_hz, text);

      // save message details
      AddDecodedMessage(tm_slot_start, cand->score, time_sec, freq_hz, text);
    }
  }

  hashtable_cleanup(10);
}

FLASHMEM bool ft8lib_InitDecoder() {
  int result = false;
  ftx_protocol_t protocol = FTX_PROTOCOL_FT8;
  float slot_period = ((protocol == FTX_PROTOCOL_FT8) ? FT8_SLOT_TIME : FT4_SLOT_TIME);
  int sample_rate = 12000;
  monitor_config_t mon_cfg = {
    .f_min = 200,
    .f_max = 3000,
    .sample_rate = sample_rate,
    .time_osr = kTime_osr,
    .freq_osr = kFreq_osr,
    .protocol = protocol
  };

  monitor = (monitor_t *)extmem_malloc(sizeof(monitor_t));
  numSamples = slot_period * sample_rate;
  signal = (float *)extmem_malloc(numSamples * sizeof(float));

  if(monitor == NULL || signal == NULL) {
    if(monitor != NULL) {
      extmem_free(monitor);
    }
    if(signal != NULL) {
      extmem_free(signal);
    }
  } else {
    hashtable_init();

    monitor_init(monitor, &mon_cfg);
    result = true;
  }

  return result;
}

FLASHMEM void ft8lib_ExitDecoder() {
  monitor_free(monitor);
  extmem_free(monitor);
  extmem_free(signal);
}

bool ft8lib_ProcessSignalData() {
  bool result = false;
  //struct tm tm_slot_start = { 0 };
  struct tm tm_slot_start = { .tm_sec = 45, .tm_min = 06, .tm_hour = 11 };
  static int count = 0;
  static int frame_pos = 0;

  if(signal != NULL && monitor != NULL) {
    // Process and accumulate audio data in a monitor/waterfall instance
    //for (int frame_pos = 0; frame_pos + monitor->block_size <= numSamples; frame_pos += monitor->block_size)
    {
      // Process the waveform data frame by frame - you could have a live loop here with data from an audio device
      monitor_process(monitor, signal + frame_pos);
      if(count < 79)
        //ft8_DrawSpectrum(&(monitor->wf.mag[count++ * 449]), 449);
        ft8_DrawSpectrum(&(monitor->wf.mag[count++ * 449 * kFreq_osr * kTime_osr]), 512);
        //delay(160);
    }
    frame_pos += monitor->block_size;

    if(frame_pos + monitor->block_size > numSamples) {
      // Decode accumulated data (containing slightly less than a full time slot)
      decode(monitor, &tm_slot_start);
      ftx_waterfall_t* me = &monitor->wf;
      int mag_size = me->num_blocks * me->time_osr * me->freq_osr * me->num_bins * sizeof(me->mag[0]);
      //Serial.print("max_blocks: "); Serial.println(me->max_blocks);
      //Serial.print("num_blocks: "); Serial.println(me->num_blocks);
      //Serial.print("time_osr: "); Serial.println(me->time_osr);
      //Serial.print("freq_osr: "); Serial.println(me->freq_osr);
      //Serial.print("num_bins: "); Serial.println(me->num_bins);
      //Serial.print("mag_size: "); Serial.println(mag_size);
      for(int i = 0; i < mag_size; i++) {
        //if(me->mag[i] > 0)
        //  Serial.println(me->mag[i]);
      }

      // Reset internal variables for the next time slot
      count = 0;
      frame_pos = 0;
      monitor_reset(monitor);

      result = true;
    }

  }

  return result;
}

void ft8lib_SetSignal(float *data, int num) {
  static int count = 0;
  static int processCount = 0;

  if(signal != NULL && monitor != NULL) {
    // transfer data to ft8_lib signal
    for(int i = 0; i < num; i++) {
      signal[count++] = data[i];
      if(count >= numSamples) {
        // done with this interval
        break;
      }
    }
    processCount += num;
    if(processCount >= monitor->block_size) {
      ft8lib_ProcessSignalData();
      processCount = 0;
    }
    if(count >= numSamples) {
      // prepare for next data set
      count = 0;
      processCount = 0;
    }
  }
}
