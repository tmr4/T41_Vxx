#pragma once

/*
 AudioOutputTCP - Streams 2-channels from connected AudioStream objects to set TCP port

Memory Usage on Teensy 4.1:
Compiled with: Smallest Code, 528MHz, Serial, 100 blocks Audio memory
  FLASH: code:259036, data:90032, headers:8300   free for files:7769096
   RAM1: variables:154272, code:220104, padding:9272   free for local variables:140640
   RAM2: variables:267840  free for malloc/new:256448
 EXTRAM: variables:1200320

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

 *** This object could be made more robust with a buffer overflow checks
     and syncing but early testing hasn't shown a need for this ***

*/

#include <Arduino.h>
#include <AudioStream.h>
#include <QNEthernet.h>
using namespace qindesign::network;

#include "debug.h"

//-------------------------------------------------------------------------------------------------------------
// Forward
//-------------------------------------------------------------------------------------------------------------

void InitEthernet(const IPAddress& ip, const IPAddress& subnet, const IPAddress& gateway);

//-------------------------------------------------------------------------------------------------------------
// Code
//-------------------------------------------------------------------------------------------------------------

class AudioOutputTCP : public AudioStream {
 public:
  AudioOutputTCP() : AudioStream(2, inputQueueArray) {}

  void begin() { enabled = true; }
	void end() {
    enabled = false;
    noInterrupts();
    clear();
    interrupts();
  }

  void setClient(EthernetClient* _client) {
    if(_client) {
      client = _client;
    } else {
      noInterrupts();
      clear();
      interrupts();
    }
  }

  // store input stream in queue
  // *** this is called from an interrupt, it can't touch QNEthernet objects ***
  void update() override {
    audio_block_t* blockL = receiveReadOnly(0);
    audio_block_t* blockR = receiveReadOnly(1);

    if(!client || !enabled || !blockL || !blockR) {
      if(blockL) release(blockL);
      if(blockR) release(blockR);
      return;
    }

    // we're enabled and have blocks to queue
    if(bufferFull()) {
      // buffer full, drop oldest block
      // *** increase buffer size if here ***
      audio_block_t* oldestL = queue[tail][0];
      audio_block_t* oldestR = queue[tail][1];
      if(oldestL) { release(oldestL); queue[tail][0] = nullptr; }
      if(oldestR) { release(oldestR); queue[tail][1] = nullptr; }
      tail = (tail + 1) & bufferMask;
    }

    queue[head][0] = blockL;
    queue[head][1] = blockR;
    head = (head + 1) & bufferMask;
  }

  // write queue data out to Ethernet
  void write() {
    if(enabled && client) {
      audio_block_t *blockL, *blockR;

      TOGGLEPROFILEPIN(PROFILER_DECODE_FT8);

      Ethernet.loop();

      TOGGLEPROFILEPIN(PROFILER_PROCESS_FRAME);
      while(client->availableForWrite() >= blockSize) {
        // take ownership of blocks
        noInterrupts();
        if(bufferEmpty()) {
          interrupts();
          break; // nothing to write
        }
        blockL = queue[tail][0];
        blockR = queue[tail][1];
        queue[tail][0] = nullptr;
        queue[tail][1] = nullptr;
        tail = (tail + 1) & bufferMask;
        interrupts();

        if(!blockL || !blockR) continue; // buffer empty or update overwrote these

        TOGGLEPROFILEPIN(PROFILER_FT8_CAT_TX);
        client->write((uint8_t *)blockL->data, blockSize / 2);
        client->write((uint8_t *)blockR->data, blockSize / 2);
        //client->writeFully((uint8_t *)blockL->data, blockSize / 2);
        //client->writeFully((uint8_t *)blockR->data, blockSize / 2);

        release(blockL);
        release(blockR);
      }
      client->flush();

      RESETPROFILEPIN(PROFILER_PROCESS_FRAME);
      RESETPROFILEPIN(PROFILER_DECODE_FT8);
      RESETPROFILEPIN(PROFILER_FT8_CAT_TX);
    }
  }

 private:
	static constexpr size_t maxBlocks = 64; // *** must be power of 2 ***
	static constexpr size_t bufferMask = maxBlocks - 1;
  static_assert((maxBlocks & (maxBlocks - 1)) == 0, "maxBlocks must be a power of 2");

  bool enabled = false;

  EthernetClient* client = nullptr;

	audio_block_t* volatile queue[maxBlocks][2] = {};
	volatile uint8_t head = 0;
	volatile uint8_t tail = 0;

  static constexpr int blockSize = AUDIO_BLOCK_SAMPLES * sizeof(int16_t) * 2;
  audio_block_t *inputQueueArray[2];

  bool bufferFull() { return ((head + 1) & bufferMask) == tail; }
  bool bufferEmpty() { return head == tail; }
  void clear() {
    audio_block_t *blockL, *blockR;
    while(tail != head) {
      blockL = queue[tail][0];
      blockR = queue[tail][1];
      if(blockL) release(blockL);
      if(blockR) release(blockR);
      tail = (tail + 1) & bufferMask;
    }
  }
};
