
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

const int kMin_score = 10; // Minimum sync score threshold for candidates
#define KMAX_CANDIDATES 140
const int kLDPC_iterations = 25;

#define KMAX_DECODED_MSGS 50

EXTMEM ftx_candidate_t candidateList[KMAX_CANDIDATES];
EXTMEM ftx_message_t decoded[KMAX_DECODED_MSGS];
EXTMEM ftx_message_t* decodedHashtable[KMAX_DECODED_MSGS];

static float *rxSignal = NULL, *txSignal = NULL;
static int numSamples;
static monitor_t *monitor;

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

#define GFSK_CONST_K 5.336446f ///< == pi * sqrt(2 / log(2))

//-------------------------------------------------------------------------------------------------------------
// Forwards
//-------------------------------------------------------------------------------------------------------------

void AddDecodedMessage(struct tm *tmSlot, int16_t score, float time_sec, float freq_hz, char *msg);

//-------------------------------------------------------------------------------------------------------------
// Decoder Code
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
  int num_candidates, num_decoded = 0;

  // find top candidates by Costas sync score and localize them in time and frequency
  num_candidates = ftx_find_candidates(wf, KMAX_CANDIDATES, candidateList, kMin_score);

  // Initialize hash table pointers
  for(int i = 0; i < KMAX_DECODED_MSGS; ++i) {
    decodedHashtable[i] = NULL;
  }

  // Go over candidates and attempt to decode messages
  for(int idx = 0; idx < num_candidates; ++idx) {
    const ftx_candidate_t* cand = &candidateList[idx];

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

    int idx_hash = message.hash % KMAX_DECODED_MSGS;
    bool found_empty_slot = false;
    bool found_duplicate = false;
    do {
      if(decodedHashtable[idx_hash] == NULL) {
        found_empty_slot = true;
      }
      else if((decodedHashtable[idx_hash]->hash == message.hash) && (0 == memcmp(decodedHashtable[idx_hash]->payload, message.payload, sizeof(message.payload)))) {
        found_duplicate = true;
      } else {
        // Move on to check the next entry in hash table
        idx_hash = (idx_hash + 1) % KMAX_DECODED_MSGS;
      }
    } while(!found_empty_slot && !found_duplicate);

    if(found_empty_slot) {
      // Fill the empty hashtable slot
      memcpy(&decoded[idx_hash], &message, sizeof(message));
      decodedHashtable[idx_hash] = &decoded[idx_hash];
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

//-------------------------------------------------------------------------------------------------------------
// Public FT8 Library Decoder Functions
//-------------------------------------------------------------------------------------------------------------

FLASHMEM bool ft8lib_InitDecoder() {
  int result = false;
  int sample_rate = 12000;
  monitor_config_t mon_cfg = {
    .f_min = 200,
    .f_max = 3000,
    .sample_rate = sample_rate,
    .time_osr = kTime_osr,
    .freq_osr = kFreq_osr,
    .protocol = FTX_PROTOCOL_FT8
  };

  monitor = (monitor_t *)extmem_malloc(sizeof(monitor_t));
  numSamples = FT8_SLOT_TIME * sample_rate;
  rxSignal = (float *)extmem_malloc(numSamples * sizeof(float));
  txSignal = (float *)extmem_malloc(numSamples * sizeof(float));

  if(monitor == NULL || rxSignal == NULL || txSignal == NULL) {
    if(monitor != NULL) {
      extmem_free(monitor);
    }
    if(rxSignal != NULL) {
      extmem_free(rxSignal);
    }
    if(txSignal != NULL) {
      extmem_free(txSignal);
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
  extmem_free(rxSignal);
  extmem_free(txSignal);
}

bool ft8lib_BufferSignal(float *buf, int sizeBuf, int offset) {
  bool result = false;

  if(rxSignal != NULL && monitor != NULL && (offset + sizeBuf <= numSamples)) {
    // transfer buffer to ft8_lib signal
    for(int i = 0; i < sizeBuf; i++) {
      rxSignal[offset + i] = buf[i];
      //Serial.println(offset + i);
    }

    result = true;
  }

  return result;
}

// Process and accumulate audio data in a monitor/waterfall instance
// Process the waveform data frame by frame
bool ft8lib_ProcessFrame(int frame) {
  bool result = false;
  int framePos = frame * monitor->block_size;

  if(rxSignal != NULL && monitor != NULL && (framePos + monitor->block_size <= numSamples)) {
    monitor_process(monitor, rxSignal + framePos);

    result = true; // success
  }

  return result;
}

uint8_t *ft8lib_GetFT8SpectrumData(int symbol) {
  uint8_t *result = NULL;

  if(rxSignal != NULL && monitor != NULL && (symbol < 79)) {
    result = &(monitor->wf.mag[symbol * monitor->wf.num_bins * kFreq_osr * kTime_osr]);
  }

  return result;
}

void ft8lib_Decode(struct tm *start) {

  if(rxSignal != NULL && monitor != NULL) {
    decode(monitor, start);

    // Reset internal variables for the next time slot
    monitor_reset(monitor);
  }
}

//-------------------------------------------------------------------------------------------------------------
// Encoder Code
//-------------------------------------------------------------------------------------------------------------

/// Computes a GFSK smoothing pulse.
/// The pulse is theoretically infinitely long, however, here it's truncated at 3 times the symbol length.
/// This means the pulse array has to have space for 3*n_spsym elements.
/// @param[in] n_spsym Number of samples per symbol
/// @param[in] b Shape parameter (values defined for FT8/FT4)
/// @param[out] pulse Output array of pulse samples
///
void gfsk_pulse(int n_spsym, float symbol_bt, float* pulse) {
  for(int i = 0; i < 3 * n_spsym; ++i) {
    float t = i / (float)n_spsym - 1.5f;
    float arg1 = GFSK_CONST_K * symbol_bt * (t + 0.5f);
    float arg2 = GFSK_CONST_K * symbol_bt * (t - 0.5f);
    pulse[i] = (erff(arg1) - erff(arg2)) / 2;
  }
}

/// Synthesize waveform data using GFSK phase shaping.
/// The output waveform will contain n_sym symbols.
/// @param[in] symbols Array of symbols (tones) (0-7 for FT8)
/// @param[in] n_sym Number of symbols in the symbol array
/// @param[in] f0 Audio frequency in Hertz for the symbol 0 (base frequency)
/// @param[in] symbol_bt Symbol smoothing filter bandwidth (2 for FT8, 1 for FT4)
/// @param[in] symbolPeriod Symbol period (duration), seconds
/// @param[in] signal_rate Sample rate of synthesized signal, Hertz
/// @param[out] signal Output array of signal waveform samples (should have space for n_sym*n_spsym samples)
///
void synth_gfsk(const uint8_t* symbols, int n_sym, float f0, float symbol_bt, float symbolPeriod, int signal_rate, float* signal) {
  const int n_spsym = (int)(0.5f + signal_rate * symbolPeriod); // Samples per symbol
  const int n_wave = n_sym * n_spsym;                            // Number of output samples
  float hmod = 1.0f;

  // Compute the smoothed frequency waveform.
  // Length = (nsym+2)*n_spsym samples, first and last symbols extended
  float dphi_peak = 2 * M_PI * hmod / n_spsym;
  float *dphi = (float *)extmem_malloc((n_wave + 2 * n_spsym) * sizeof(float));
  float *pulse = (float *)extmem_malloc((3 * n_spsym) * sizeof(float));
  float phi = 0;
  int n_ramp = n_spsym / 8;

  // Shift frequency up by f0
  for(int i = 0; i < n_wave + 2 * n_spsym; ++i) {
    dphi[i] = 2 * M_PI * f0 / signal_rate;
  }

  gfsk_pulse(n_spsym, symbol_bt, pulse);

  for(int i = 0; i < n_sym; ++i) {
    int ib = i * n_spsym;

    for(int j = 0; j < 3 * n_spsym; ++j) {
      dphi[j + ib] += dphi_peak * symbols[i] * pulse[j];
    }
  }

  // Add dummy symbols at beginning and end with tone values equal to 1st and last symbol, respectively
  for(int j = 0; j < 2 * n_spsym; ++j) {
    dphi[j] += dphi_peak * pulse[j + n_spsym] * symbols[0];
    dphi[j + n_sym * n_spsym] += dphi_peak * pulse[j] * symbols[n_sym - 1];
  }

  // Calculate and insert the audio waveform
  for(int k = 0; k < n_wave; ++k) { // Don't include dummy symbols
    signal[k] = sinf(phi);
    phi = fmodf(phi + dphi[k + n_spsym], 2 * M_PI);
  }

  // Apply envelope shaping to the first and last symbols
  for(int i = 0; i < n_ramp; ++i) {
    float env = (1 - cosf(2 * M_PI * i / (2 * n_ramp))) / 2;
    signal[i] *= env;
    signal[n_wave - 1 - i] *= env;
  }

  extmem_free(dphi);
  extmem_free(pulse);
}

//-------------------------------------------------------------------------------------------------------------
// Public FT8 Library Encoder Functions
//-------------------------------------------------------------------------------------------------------------

// minimum signal size = numTones * symbolPeriod * sampleRate = 79 * 0.16 * 12000 = 151680
FLASHMEM bool ft8lib_GenFT8(char *message, float frequency) {
  bool result = false;
  int sampleRate = 12000;
  //int sampleRate = 24000;
  ftx_message_t msg;
  uint8_t tones[79]; // array of 79 tones (symbols)

  // pack the text data into binary message
  if(ftx_message_encode(&msg, NULL, message) != FTX_MESSAGE_RC_OK) {
    Serial.println("ftx_message_encode failed");
    return result;
  }

  // encode the binary message as a sequence of FSK tones
  ft8_encode(msg.payload, tones);

  if(txSignal != NULL) {
    // convert the FSK tones into an audio signal
    // FT8_SYMBOL_BT 2.0f ///< symbol smoothing filter bandwidth factor (BT)
    synth_gfsk(tones, FT8_NN, frequency, 2.0f, FT8_SYMBOL_PERIOD, sampleRate, txSignal);

    // save it as wav file
    //save_wav(signal, num_total_samples, sampleRate, wav_path);

    result = true;
  }

  return result;
}

float *ft8lib_GetSignal() {
  return txSignal;
}
