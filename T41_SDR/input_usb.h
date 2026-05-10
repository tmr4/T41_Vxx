#pragma once

/*
 AudioUSBReceiver - Teensy AudioStream object

 Receives:
   16 x 512-byte IQ blocks
   1 x 512-byte sync block

 State driven

 Nonblocking
 I2S Audio-driven
 Circular buffer

*/

#include <Arduino.h>
#include <AudioStream.h>

#include "debug.h"

//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

extern "C" {
  int usb_serial_read(void *buffer,  uint32_t size);
  int usb_serial2_read(void *buffer, uint32_t size);
  int usb_serial3_read(void *buffer, uint32_t size);

  int usb_serial_available(void);
  int usb_serial2_available(void);
  int usb_serial3_available(void);
}

//-------------------------------------------------------------------------------------------------------------
// Forwards
//-------------------------------------------------------------------------------------------------------------

extern void SendMsg(const char *msg, int value);

//-------------------------------------------------------------------------------------------------------------
// Code
//-------------------------------------------------------------------------------------------------------------

template< int (*available)(), int (*read)(void *, uint32_t) >
class AudioUSBReceiver : public AudioStream {
public:
  AudioUSBReceiver() : AudioStream(0, nullptr) {}

	void begin() {
		clear();
		enabled = true;
	}
	//int available() {}
	void clear() {}
	void end() {
		enabled = false;
	}

  void update() override {
    static uint8_t buf[512];
    TOGGLEPROFILEPIN(PROFILER_DECODE_FT8);
    if(t41.RemoteStatus != REMOTE_CONNECTED) {
      while(available()) read(buffer[head], 1);
      RESETPROFILEPIN(PROFILER_DECODE_FT8);
      return;
    }
    if(available() < blockSize) {
      RESETPROFILEPIN(PROFILER_DECODE_FT8);
      return;
    }
    audio_block_t *blockL = allocate();
    audio_block_t *blockR = allocate();
    if(!blockL || !blockR) {
      TOGGLEPROFILEPIN(PROFILER_PROCESS_FRAME);
      if(blockL) release(blockL);
      if(blockR) release(blockR);
      RESETPROFILEPIN(PROFILER_DECODE_FT8);
      return;
    }
    if(read(buf, blockSize) == blockSize) {
      TOGGLEPROFILEPIN(PROFILER_FT8_REMOTE_RX);
      TOGGLEPROFILEPIN(PROFILER_PROCESS_FRAME);
      memcpy((void *)blockL->data, buf, blockSize / 2);
      memcpy((void *)blockR->data, &buf[blockSize / 2], blockSize / 2);
      transmit(blockL, 0);
      transmit(blockR, 1);
    }
    release(blockL);
    release(blockR);
    RESETPROFILEPIN(PROFILER_PROCESS_FRAME);
    RESETPROFILEPIN(PROFILER_DECODE_FT8);
    RESETPROFILEPIN(PROFILER_FT8_REMOTE_RX);

    /*
    TOGGLEPROFILEPIN(PROFILER_DECODE_FT8);
    if(t41.RemoteStatus != REMOTE_CONNECTED) {
      while(available() > 0) read(buffer[head], 1);
      RESETPROFILEPIN(PROFILER_DECODE_FT8);
      return;
    }
    //if(bufFull()) return;
    if(available() < blockSize) {
      RESETPROFILEPIN(PROFILER_DECODE_FT8);
      return;
    }

    // put block in buffer
    int received = read(buffer[head], blockSize);
    if(received == blockSize) {
      TOGGLEPROFILEPIN(PROFILER_FT8_REMOTE_RX);
      head = (head + 1) & blockMask;
    } else {
      // didn't receive a full block
      // do some partial processing
      // *** TODO: what exactly? ***
    }

    if(state == LOCKED) {
      //if(!checkSync()) return;

      audio_block_t *blockL = allocate();
      audio_block_t *blockR = allocate();

      if(!blockL || !blockR) {
        if(blockL) release(blockL);
        if(blockR) release(blockR);
        RESETPROFILEPIN(PROFILER_DECODE_FT8);
        RESETPROFILEPIN(PROFILER_FT8_REMOTE_RX);
        return;
      }
      TOGGLEPROFILEPIN(PROFILER_PROCESS_FRAME);

      memcpy(blockL, buffer[tail], blockSize / 2);
      memcpy(blockR, &buffer[tail][blockSize / 2], blockSize / 2);

      transmit(blockL, 0);
      transmit(blockR, 1);

      release(blockL);
      release(blockR);

      // consume block
      tail = (tail + 1) & blockMask;
      remaining--;
    } else {
      //sync();
    }
    RESETPROFILEPIN(PROFILER_PROCESS_FRAME);
    RESETPROFILEPIN(PROFILER_DECODE_FT8);
    RESETPROFILEPIN(PROFILER_FT8_REMOTE_RX);
    */
  }

private:
  bool enabled = false;
  static constexpr uint32_t syncWord = 0xA55AA55A;

  static constexpr int blockSize = AUDIO_BLOCK_SAMPLES * sizeof(int16_t) * 2;
  static constexpr int frameBlocks = 16;

  static constexpr int bufferBlocks = 32;
  static constexpr int blockMask = (bufferBlocks - 1);

  alignas(32) uint8_t buffer[bufferBlocks][blockSize]; // __attribute__((aligned (32)));

  volatile size_t head = 0;
  volatile size_t tail = 0;

  enum { LOCKED, SCAN, RESYNC };

  uint32_t state = LOCKED;

  int remaining = frameBlocks;

  uint32_t syncOffset = 0;

  bool bufFull() { return ((head + 1) & blockMask) == tail; }
  bool bufEmpty() { return head == tail; }
  bool checkSync() {
    bool result = false;

    if(remaining == 0) {
      // sync block?
      if(peek() == syncWord) {
        // yes, consume it
        tail = (tail + 1) & blockMask;

        remaining = frameBlocks;
        result = true;
      } else {
        state = SCAN;
      }
    }
    return result;
  }

  uint32_t peek(size_t offset = 0) { return *(uint32_t*)(&buffer[tail][offset]); }

  void sync() {
    switch(state) {
      case SCAN:
        // check for slippage over a couple of words
        if(resync(8)) {
          SendMsg("Slip before: %d;", syncOffset);
        } else {
          state = RESYNC;
        }
        break;

      case RESYNC:
        if(resync(128)) {
          SendMsg("Resync'd at: %d;", syncOffset);
        } else {
          state = RESYNC;
        }
        break;
    }
  }

  bool resync(size_t iterations) {
    bool result = false;
    size_t i;

    // check for slippage
    for(i = 1; i <= iterations; i++) {
      if(peek(i) == syncWord) {
        // we've slipped
        // I haven't seen this in practice
        // we're no longer block aligned, not good for efficiency
        // can't do the typical tail = (tail + 1) & blockMask;

        // *** TODO: warn and work up strategy ***
        remaining = frameBlocks;
        state = LOCKED;
        result = true;
        break;
      }
    }

    syncOffset = i;
    return result;
  }
};

using AudioInputUSBSerial = AudioUSBReceiver< usb_serial_available, usb_serial_read >;

using AudioInputUSBSerial1 = AudioUSBReceiver< usb_serial2_available, usb_serial2_read >;

using AudioInputUSBSerial2 = AudioUSBReceiver< usb_serial3_available, usb_serial3_read >;
