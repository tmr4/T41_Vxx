#pragma once

/*
 AudioOutputUDP - Streams 2-channels from connected AudioStream objects to set UDP port

 Works with AudioInputUDP

*/

#include <Arduino.h>
#include <AudioStream.h>

#include <QNEthernet.h>
using namespace qindesign::network;

#include "debug.h"

//-------------------------------------------------------------------------------------------------------------
// Code
//-------------------------------------------------------------------------------------------------------------

class AudioOutputUDP : public AudioStream {
 public:
  AudioOutputUDP() : AudioStream(2, inputQueueArray) {}

  void begin() { enabled = true; }
	void end() {
    enabled = false;
    clear();
  }

  void setClient(EthernetUDP* _client) {
    if(!_client) clear();
    client = _client;
  }

  // store input stream in queue
  // *** this is called from an interrupt, it can't touch QNEthernet objects ***
  // *** only touches head! ***
  void update() override {
    audio_block_t* blockL = receiveReadOnly(0);
    audio_block_t* blockR = receiveReadOnly(1);

    if(!client || !enabled || !blockL || !blockR || bufferFull()) {
      if(blockL) release(blockL);
      if(blockR) release(blockR);
      return;
    }

    queue[head][0] = blockL;
    queue[head][1] = blockR;
    head = (head + 1) & bufferMask;
  }

  // write queue data out to UDP
  void write() {
    if(enabled && client) {
      audio_block_t *blockL, *blockR;
      size_t h;
      bool bFull;
      unsigned long start;
      bool flag = true;

      SETPROFILEPIN(PROFILER_ENTRY);

      // lock stuff for this loop
      noInterrupts();
      h = head;
      bFull = bufferFull();
      interrupts();

      // *** buffer full check (usually happens on system glitch) ***
      // I've tested just dropping the oldest block and adding the new one.
      // The system recovers after the glitch, but spends sometime doing
      // the swap with no real gain. The damage (audio artifact) is already done.
      // Better is to just clear the entire buffer and allow the system to recover
      // faster instead of trying to force through old data. Setting a flag
      // to note buffer was fully during an update allows clear() to run from
      // an interrupt.
      if(bFull) {
        //telemetry.bufferClearEvent(h, tail, available);
        clear();
        h = 0;
      } else {
        //telemetry.preLoopCheck(h, tail, available);
      }

      while(flag) {
        start = micros();

        //telemetry.inLoopCheck(h, tail, available);

        if(h == tail) { // empty check
          break; // nothing to write
        }

        // take ownership of blocks
        blockL = queue[tail][0];
        blockR = queue[tail][1];
        queue[tail][0] = nullptr;
        queue[tail][1] = nullptr;
        tail = (tail + 1) & bufferMask;

        if(!blockL || !blockR) continue; // buffer empty

        SETPROFILEPIN(PROFILER_RX_TX);
        if(client->beginPacket(clientIP, dataPort)) {
          client->write((uint8_t*)blockL->data, blockSize / 2);
          client->write((uint8_t*)blockR->data, blockSize / 2);
          client->write((uint8_t*)&sequenceCounter, sizeof(uint32_t));
          client->endPacket();
          //Serial.println(sequenceCounter);
          ++sequenceCounter;
        }
        if(micros() - start > 50) Serial.println("long write in AudioOutputTCP");

        release(blockL);
        release(blockR);
      }

      RESETPROFILEPIN(PROFILER_ENTRY);
      RESETPROFILEPIN(PROFILER_RX_TX);
    }
  }

 private:
	static constexpr size_t maxBlocks = 64; // *** must be power of 2 ***
	static constexpr size_t bufferMask = maxBlocks - 1;
  static_assert((maxBlocks & (maxBlocks - 1)) == 0, "maxBlocks must be a power of 2");

  bool enabled = false;

  EthernetUDP* client = nullptr;
  const IPAddress clientIP{192, 168, 1, 101};
  uint16_t dataPort = 8001;
  uint32_t sequenceCounter = 0;

	audio_block_t* volatile queue[maxBlocks][2] = {};
	volatile size_t head = 0;
	size_t tail = 0;

  static constexpr int blockSize = AUDIO_BLOCK_SAMPLES * sizeof(int16_t) * 2;
  audio_block_t *inputQueueArray[2];

  bool bufferFull() { return ((head + 1) & bufferMask) == tail; }
  //bool bufferEmpty() { return head == tail; }
  void clear() {
    audio_block_t *blockL, *blockR;

    noInterrupts();
    while(tail != head) {
      blockL = queue[tail][0];
      blockR = queue[tail][1];
      if(blockL) release(blockL);
      if(blockR) release(blockR);
      tail = (tail + 1) & bufferMask;
    }
    head = tail = 0;
    interrupts();
  }
};
