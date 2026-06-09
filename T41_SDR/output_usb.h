#pragma once

/*
 AudioOutputHostSerial - Streams 2-channels from connected input objects to specified USB Host serial object

T41 timing (w/ T41 standard testing input, Auto NF):
  * 2-channel input, 512-bytes total, written directly to USB Host serial (not buffered)
    * { L-channel block, R-channel block } or { 256-bytes left channel, 256-bytes right channel }
  * update() run every 667us (2.9ms /44.1kHz * 192kHz)
  * ~205us to write 512-bytes to USB Host serial
  * ~2.8ms to process the 16 blocks of data required to form a frame for display
    * T41 take twice as long to process data due to the longer time required to
      write to USB Host serial than the remote needs to read the same amount of
      data from USB serial.
  * This time adds to the time to complete one update of display (frame):
    * ~150ms or ~6.7 frames/sec with remote attached
    * ~96ms w/o remote ~10.4 frames/sec
  * Notes:
    * The T41 and remote prepare to render the next frame immediately after completing the
      previous frame. The different frame rates mean that the two units aren't rendering the same
      data slices at any given time.
    * The T41 and remote are running at different clock rates, 528MHz for the T41 for Teensy
      longevity and 600MHz on the remote due to AP instability at 528MHz (Teensy chip voltage issue)
    * The objects seemlessly handle disconnects in most cases.

 Works with AudioInputSerial

 *** This object could be made more robust with a buffer and syncing but
     early testing hasn't shown a need for this ***

*/

#include <USBHost_t36.h>

#include "connectBase.h"
#include "debug.h"

//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

class AudioOutputHostSerial : public AudioConnectBase {
public:
  AudioOutputHostSerial() : AudioConnectBase(2, inputQueueArray) {}

  void init(USBHost* _host, USBSerial_BigBuffer* _serial) {
    host = _host;
    serial = _serial;
  }
	void begin() override {
    if(!host || !serial) {
      enabled = false;
    } else {
      enabled = true;
    }
  }
	//void end() { enabled = false;	}

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
    host->Task();
    if(serial->availableForWrite() < blockSize) {
      release(blockL);
      release(blockR);
      RESETPROFILEPIN(PROFILER_PROCESS_FRAME);
      RESETPROFILEPIN(PROFILER_DECODE_FT8);
      return;
    }

    TOGGLEPROFILEPIN(PROFILER_OTHER);
    serial->write((uint8_t *)blockL->data, blockSize / 2);
    serial->write((uint8_t *)blockR->data, blockSize / 2);
    host->Task();

    release(blockL);
    release(blockR);
    RESETPROFILEPIN(PROFILER_PROCESS_FRAME);
    RESETPROFILEPIN(PROFILER_DECODE_FT8);
    RESETPROFILEPIN(PROFILER_OTHER);
  }

private:
  bool enabled = false;

  USBHost* host = nullptr;
  USBSerial_BigBuffer* serial = nullptr;

  static constexpr int blockSize = AUDIO_BLOCK_SAMPLES * sizeof(int16_t) * 2;
  audio_block_t *inputQueueArray[2];
};
