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

  void setClient(EthernetUDP* _client, IPAddress ip = IPAddress(0, 0, 0, 0), uint16_t port = 0) {
    if(!_client) {
      clear();
      return;
    }
    client = _client;
    clientIP = ip;
    dataPort = port;
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

      SETPROFILEPIN(PROFILER_ENTRY);

      // lock stuff for this loop
      noInterrupts();
      h = head;
      bFull = bufferFull();
      interrupts();

      if(bFull) {
        clear();
        h = 0;
      } else {
      }

      // write while queue has data
      while(h != tail) {
        // take ownership of blocks
        blockL = queue[tail][0];
        blockR = queue[tail][1];
        queue[tail][0] = nullptr;
        queue[tail][1] = nullptr;
        tail = (tail + 1) & bufferMask;

        if(!blockL || !blockR) break; // normally this just duplicates te h!=tail check

        SETPROFILEPIN(PROFILER_RX_TX);
        if(client->beginPacket(clientIP, dataPort)) {
          client->write((uint8_t*)blockL->data, blockSize / 2);
          client->write((uint8_t*)blockR->data, blockSize / 2);
          client->write((uint8_t*)&sequenceCounter, sizeof(uint32_t));
          client->endPacket();
          ++sequenceCounter;
        }

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
  IPAddress clientIP{0, 0, 0, 0};
  uint16_t dataPort = 0;
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
