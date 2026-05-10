#pragma once

/*
 AudioOutputHostSerial - Streams 2-channels to specified USB Host serial object

 Works with AudioInputSerial

 *** This object could be made more robust with a buffer and syncing but
     early testing hasn't shown a need for this ***
 */

#include <Arduino.h>
#include <AudioStream.h>
#include <USBHost_t36.h>

#include "debug.h"

//-------------------------------------------------------------------------------------------------------------
// Code
//-------------------------------------------------------------------------------------------------------------

class AudioOutputHostSerial : public AudioStream {
public:
  AudioOutputHostSerial() : AudioStream(2, inputQueueArray) {}

  void init(USBHost* host, USBSerial_BigBuffer* serial) {
    _host = host;
    _serial = serial;
  }
	void begin() {
    if(!_host || !_serial) {
      enabled = false;
    } else {
      enabled = true;
    }
  }
	void end() { enabled = false;	}

  void update() override {
    audio_block_t *blockL, *blockR;

    TOGGLEPROFILEPIN(PROFILER_DECODE_FT8);
    blockL = receiveReadOnly(0);
    blockR = receiveReadOnly(1);

    if(!enabled || !blockL || !blockR) {
      if(blockL) release(blockL);
      if(blockR) release(blockR);
      RESETPROFILEPIN(PROFILER_DECODE_FT8);
      return;
    }

    TOGGLEPROFILEPIN(PROFILER_PROCESS_FRAME);
    _host->Task();
    if(_serial->availableForWrite() < blockSize) {
      release(blockL);
      release(blockR);
      RESETPROFILEPIN(PROFILER_PROCESS_FRAME);
      RESETPROFILEPIN(PROFILER_DECODE_FT8);
      return;
    }

    TOGGLEPROFILEPIN(PROFILER_FT8_CAT_TX);
    _serial->write((uint8_t *)blockL->data, blockSize / 2);
    _serial->write((uint8_t *)blockR->data, blockSize / 2);
    _host->Task();

    release(blockL);
    release(blockR);
    RESETPROFILEPIN(PROFILER_PROCESS_FRAME);
    RESETPROFILEPIN(PROFILER_DECODE_FT8);
    RESETPROFILEPIN(PROFILER_FT8_CAT_TX);
  }

private:
  bool enabled = false;

  USBHost* _host = nullptr;
  USBSerial_BigBuffer* _serial = nullptr;

  static constexpr int blockSize = AUDIO_BLOCK_SAMPLES * sizeof(int16_t) * 2;
  audio_block_t *inputQueueArray[2];
};
