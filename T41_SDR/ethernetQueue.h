#pragma once

/*

EthernetQueue - Queues 2-channels to/from related AudioStreams and a UDP data port

Data Structure:
  * 2-channel input, 512-bytes total, buffered and then written to UDP data port
    * { L-channel block, R-channel block } or { 256-bytes left channel, 256-bytes right channel }

 Works with AudioInputFromQueue and AudioOutputToQueue

*/

#include <Arduino.h>
#include <AudioStream.h>

#include <QNEthernet.h>
using namespace qindesign::network;

//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

class AudioOutputToQueue;

class EthernetQueue {
 public:
  EthernetQueue(uint16_t dPort = 8001) : dataPort(dPort) {}

  void init(AudioOutputToQueue* s) {
    if(s) {
      audioStream = s;
      udpClient.begin(dataPort);
      enabled = true;
    }
  }

	void begin() { if(audioStream) enabled = true; };
	void end() {
    enabled = false;
    clear();
  }

  bool pushBlocks(audio_block_t* blockL, audio_block_t* blockR);
  bool popBlocks(audio_block_t*& blockL, audio_block_t*& blockR);

  void writeFromQueue();
  void readToQueue();

  bool isQueueFull() { return ((head + 1) & bufferMask) == tail; }
  bool isQueueEmpty() { return head == tail; }

private:
  bool enabled = false;

  uint16_t dataPort;
  EthernetUDP udpClient{8}; // IQ data channel
  const IPAddress clientIP{192, 168, 1, 101};

  // *** sequence counters are useful for testing, but not needed generally ***
  //uint32_t sequenceCounter = 0;
  //uint32_t lastSequenceCounter = 0;
  //uint32_t expectedSequenceCounter = 0;

  AudioOutputToQueue* audioStream;

  static constexpr int blockSize = AUDIO_BLOCK_SAMPLES * sizeof(int16_t) * 2;
	static constexpr size_t maxBlocks = 64; // *** must be power of 2 ***
	static constexpr size_t bufferMask = maxBlocks - 1;
  static_assert((maxBlocks & (maxBlocks - 1)) == 0, "maxBlocks must be a power of 2");

	audio_block_t* volatile queue[maxBlocks][2] = {};
	volatile size_t head = 0;
  volatile size_t tail = 0;

  void clear();

  void allocate(audio_block_t*& blockL, audio_block_t*& blockR);
  void release(audio_block_t* blockL, audio_block_t* blockR);
};
