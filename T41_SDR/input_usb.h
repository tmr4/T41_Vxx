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

extern void SendMsg(const char *msg, int value);

extern "C" {
  int usb_serial_read(void *buffer,  uint32_t size);
  int usb_serial2_read(void *buffer, uint32_t size);
  int usb_serial3_read(void *buffer, uint32_t size);

  int usb_serial_available(void);
  int usb_serial2_available(void);
  int usb_serial3_available(void);
}

template< int (*available)(), int (*read)(void *, uint32_t) >
class AudioUSBReceiver : public AudioStream {
public:
  AudioUSBReceiver() : AudioStream(0, nullptr) {}

  void begin() {}

  void update(void) override {
    if(bufFull()) return;
    if(available() < blockSize) return;

    // put block in buffer
    int received = read(buffer[head], blockSize);
    if(received == blockSize) {
      head = (head + 1) & blockMask;
    } else {
      // didn't receive a full block
      // do some partial processing
      // *** TODO: what exactly? ***
    }

    if(state == LOCKED) {
      if(!checkSync()) return;

      audio_block_t *blockL = receiveReadOnly(0);
      audio_block_t *blockR = receiveReadOnly(1);

      if(!blockL || !blockR) {
        if(blockL) release(blockL);
        if(blockR) release(blockR);
        return;
      }

      memcpy(blockL, buffer[tail], blockSize);
      memcpy(blockR, &buffer[tail][256], blockSize);

      transmit(blockL, 0);
      transmit(blockR, 1);

      release(blockL);
      release(blockR);

      // consume block
      tail = (tail + 1) & blockMask;
      remaining--;
    } else {
      sync();
    }
  }

private:
  static constexpr uint32_t syncWord = 0xA55AA55A;

  static constexpr int blockSize = AUDIO_BLOCK_SAMPLES * sizeof(int16_t);
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
