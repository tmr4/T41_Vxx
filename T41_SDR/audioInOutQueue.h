#pragma once

/*

Streams 2-channels of data from/to connected AudioStream objects to/from a queue

Data Structure:
  * 2-channel input, 512-bytes total
    * { L-channel block, R-channel block } or { 256-bytes left channel, 256-bytes right channel }

*/

#include <Arduino.h>
#include <AudioStream.h>

//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

class EthernetBridgeQueue;

class AudioInputFromQueue : public AudioStream {
 public:
  AudioInputFromQueue() : AudioStream(0, nullptr) {}

  void init(EthernetBridgeQueue* q) { queue = q; }

	void begin() { if(queue) enabled = true; };
	void end() { enabled = false; }

private:
  bool enabled = false;
  EthernetBridgeQueue* queue = nullptr;

  void update() override;
};

class AudioOutputToQueue : public AudioStream {
 public:
  AudioOutputToQueue() : AudioStream(2, inputQueueArray) {}

  void init(EthernetBridgeQueue* q) { queue = q; }

	void begin() { if(queue) enabled = true; };
	void end() { enabled = false; }

  void allocateBlocks(audio_block_t*& blockL, audio_block_t*& blockR) {
    blockL = allocate();
    blockR = allocate();
  }

  void releaseBlocks(audio_block_t* blockL, audio_block_t* blockR) {
    if(blockL) release(blockL);
    if(blockR) release(blockR);
  }

private:
  bool enabled = false;
  EthernetBridgeQueue* queue = nullptr;

  void update() override;

  audio_block_t* inputQueueArray[2];
};
