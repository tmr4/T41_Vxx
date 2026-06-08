#pragma once

/*
 AudioOutputTCP - Streams 2-channels from connected AudioStream objects to set TCP port

T41 timing (w/ T41 standard testing input, Auto NF):
  * update() runs every 667us (2.9ms /44.1kHz * 192kHz), 2-channel input
    * pointers to input blocks stored in circular buffer once enabled and connected
  * Once enabled and connected, buffered data, 512-bytes total, written directly to TCP port
    * { L-channel block, R-channel block } or { 256-bytes left channel, 256-bytes right channel }
    * Unlike USB object, this can't be done in update since QNEthernet objects can't be accessed from interrupt state
  * write() must be called with sufficient frequency to avoid buffer overflow or data will be lost
    * A similar frequency to update() maintains smooth data flow and minimizes Audio memory needs
  * ~10us to write 512-bytes to TCP port
  * ~2ms to process the 16 blocks of data required to form a frame for display
    * This is faster than with USB transfers as the TCP write is faster than USB.
  * This time adds to the time to complete one update of display (frame):
    * ~106ms or ~9.4 frames/sec with remote attached
    * ~99ms w/o remote ~10.1 frames/sec
  * Notes:
    * Ethernet transport: on average one 512-byte packets transmitted every 500-700us
    * The T41 and remote prepare to render the next frame immediately after completing the
      previous frame. The different frame rates mean that the two units aren't rendering the same
      data slices at any given time.
    * The T41 and remote are running at different clock rates, 528MHz for the T41 for Teensy
      longevity and 600MHz on the remote due to AP instability at 528MHz (Teensy chip voltage issue)
    * The objects seemlessly handle disconnects in most cases. A long disconnect seems to require
      a longer time to reconnect.

 Works with AudioInputTCP

 *** see AudioInputTCP for notes on a pause that occurs periodically when using these objects ***

*/

#include <Arduino.h>
#include <AudioStream.h>

#include <QNEthernet.h>
using namespace qindesign::network;

#include "debug.h"
#include "telemetry.h"

//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

//-------------------------------------------------------------------------------------------------------------
// Code
//-------------------------------------------------------------------------------------------------------------

class AudioOutputTCP : public AudioStream {
 public:
  AudioOutputTCP() : AudioStream(2, inputQueueArray) {}

  void begin() { enabled = true; }
	void end() {
    enabled = false;
    clear();
  }

  void setClient(EthernetClient* _client) {
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

  // write queue data out to Ethernet
  // *** EthernetClient::availableForWrite and flush include a call
  //     to Ethernet.loop so an extra one here is not needed ***
  void write() {
    if(enabled && client) {
      audio_block_t *blockL, *blockR;
      int available = client->availableForWrite();
      size_t h;
      bool bFull;
      unsigned long start;
      bool flag = available < blockSize;

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
        telemetry.bufferClearEvent(h, tail, available);
        clear();
        h = 0;
      } else {
        telemetry.preLoopCheck(h, tail, available);
      }

      while(available >= blockSize) {
        start = micros();

        telemetry.inLoopCheck(h, tail, available);

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
        //client->write((uint8_t *)blockL->data, blockSize / 2);
        //client->write((uint8_t *)blockR->data, blockSize / 2);
        client->writeFully((uint8_t *)blockL->data, blockSize / 2);
        client->writeFully((uint8_t *)blockR->data, blockSize / 2);
        if(micros() - start > 50) Serial.println("long write in AudioOutputTCP");

        release(blockL);
        release(blockR);
        available -= 512;
        //available = client->availableForWrite();
      }

      //client->flush(); // includes call to Ethernet.loop()
      if(flag) {
        Ethernet.loop();
      } else {
        client->flush(); // includes call to Ethernet.loop()
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

  EthernetClient* client = nullptr;

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
