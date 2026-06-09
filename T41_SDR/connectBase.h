#pragma once

#include <Arduino.h>
#include <AudioStream.h>

//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

class AudioConnectBase : public AudioStream {
public:
  AudioConnectBase(unsigned char ninput, audio_block_t **iqueue) : AudioStream(ninput, iqueue) {}

	virtual void begin() { enabled = true; };
	virtual void end() { enabled = false; }
  virtual void readToQueue() {}
  virtual void writeToQueue() {}

protected:
  bool enabled = false;
};

template <typename H, typename T>
class AudioConnectBuffered : public AudioConnectBase {
public:
  AudioConnectBuffered(unsigned char ninput, audio_block_t **iqueue) : AudioConnectBase(ninput, iqueue) {}

protected:
	static constexpr size_t maxBlocks = 64; // *** must be power of 2 ***
	static constexpr size_t bufferMask = maxBlocks - 1;
  static_assert((maxBlocks & (maxBlocks - 1)) == 0, "maxBlocks must be a power of 2");

	audio_block_t* volatile queue[maxBlocks][2] = {};
	T tail = 0;
	H head = 0;

  bool bufferFull() { return ((head + 1) & bufferMask) == tail; }
  //bool bufferEmpty() { return head == tail; }
  void clear() {
    audio_block_t *blockL, *blockR;

    noInterrupts();
    for(size_t i = 0; i < maxBlocks; i++) {
      blockL = queue[i][0];
      blockR = queue[i][1];
      if(blockL) release(blockL);
      if(blockR) release(blockR);
      queue[i][0] = nullptr;
      queue[i][1] = nullptr;
    }
    head = tail = 0;
    interrupts();
  }
};
