#pragma once

/*
 AudioInputTCP - Streams 2-channels from set TCP port to connected AudioStream objects

Memory Usage on Teensy 4.1:
Compiled with: Smallest Code, 528MHz, Serial, 100 blocks Audio memory
  FLASH: code:270304, data:91056, headers:8300   free for files:7756804
   RAM1: variables:166432, code:231960, padding:30184   free for local variables:95712
   RAM2: variables:267840  free for malloc/new:256448
 EXTRAM: variables:480320

Remote timing (w/ T41 standard testing input, Auto NF):
  * Once enabled and connected, data read from TCP port, 512-bytes total, directly to blocks allocated from Audio memory
    * { L-channel block, R-channel block } or { 256-bytes left channel, 256-bytes right channel }
    * Pointers to these blocks stored in circular buffer
  * read() must be called with sufficient frequency to avoid TCP buffer overflow
    * A similar frequency to update() maintains smooth data flow and minimizes Audio memory needs
  * update() run every 667us (2.9ms /44.1kHz * 192kHz)
    * queued input { 256-bytes left channel } and { 256-bytes right channel } sent to connected objects
  * ~15us to read 512-bytes from TCP port
  * ~1.4ms to process the 16 blocks of data required to form a frame for display
  * Time to complete one update of display (frame):
    * ~75ms or ~13.4 frames/sec
  * Notes:
    * Ethernet transport: on average two 512-byte packets arrive ~90us apart every 2ms
    * The T41 and remote prepare to render the next frame immediately after completing the
      previous frame. The different frame rates mean that the two units aren't rendering the same
      data slices at any given time.
    * The T41 and remote are running at different clock rates, 528MHz for the T41 for Teensy
      longevity and 600MHz on the remote due to AP instability at 528MHz (Teensy chip voltage issue)
    * The objects seemlessly handle disconnects in most cases. A long disconnect seems to require
      a longer time to reconnect.

 Works with AudioOutputTCP

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

class AudioInputTCP : public AudioStream {
public:
  AudioInputTCP() : AudioStream(0, nullptr) {}

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

  // send queued data to stream
  // *** this is called from an interrupt, it can't touch QNEthernet objects ***
  // *** only touches tail! ***
  void update() override {
    audio_block_t *blockL, *blockR;

    if(bufferEmpty()) return; // nothing to stream

    if(enabled) {
      // we have blocks to stream
      blockL = queue[tail][0];
      blockR = queue[tail][1];
      transmit(blockL, 0);
      transmit(blockR, 1);
      release(blockL);
      release(blockR);
      queue[tail][0] = nullptr;
      queue[tail][1] = nullptr;
      tail = (tail + 1) & bufferMask;
    }
  }

  // read TCP data into queue
  void read() {
    if(client) {
      Ethernet.loop();

      if(enabled) {
        audio_block_t *blockL, *blockR;
        int n;

        TOGGLEPROFILEPIN(PROFILER_DECODE_FT8);
        while((client->available() >= blockSize)) {
          TOGGLEPROFILEPIN(PROFILER_PROCESS_FRAME);
          // we have sufficient data to queue
          blockL = allocate();
          blockR = allocate();
          if(!blockL || !blockR) {
            if(blockL) release(blockL);
            if(blockR) release(blockR);
            //break;
            RESETPROFILEPIN(PROFILER_PROCESS_FRAME);
            RESETPROFILEPIN(PROFILER_DECODE_FT8);
            return;
          }

          TOGGLEPROFILEPIN(PROFILER_FT8_REMOTE_RX);
          n = client->read((uint8_t *)blockL->data, blockSize / 2);
          n += client->read((uint8_t *)blockR->data, blockSize / 2);
          if(n < blockSize) {
            // read error
            release(blockL);
            release(blockR);
          } else {
            noInterrupts();
            if(bufferFull()) {
              // buffer full, drop oldest block
              // *** increase buffer size if here ***
              audio_block_t* oldestL = queue[tail][0];
              audio_block_t* oldestR = queue[tail][1];
              if(oldestL) { release(oldestL); queue[tail][0] = nullptr; }
              if(oldestR) { release(oldestR); queue[tail][1] = nullptr; }
              tail = (tail + 1) & bufferMask;
              TOGGLEPROFILEPIN(PROFILER_FT8_CAT_TX);
            }
            interrupts();

            queue[head][0] = blockL;
            queue[head][1] = blockR;
            head = (head + 1) & bufferMask;
          }
        }
      } else {
        uint8_t dump[512];
        // empty Ethernet buffer
        while(client->available() >= 512) {
          TOGGLEPROFILEPIN(PROFILER_PROCESS_FRAME);
          client->read(dump, 512);
        }
        // get the last bit
        while(client->available()) {
          TOGGLEPROFILEPIN(PROFILER_PROCESS_FRAME);
          client->read(dump, 1);
        }
      }
    }

    RESETPROFILEPIN(PROFILER_PROCESS_FRAME);
    RESETPROFILEPIN(PROFILER_DECODE_FT8);
    RESETPROFILEPIN(PROFILER_FT8_REMOTE_RX);
    RESETPROFILEPIN(PROFILER_FT8_CAT_TX);
  }

private:
	static constexpr size_t maxBlocks = 64; // *** must be power of 2 ***
	static constexpr size_t bufferMask = maxBlocks - 1;
  static_assert((maxBlocks & (maxBlocks - 1)) == 0, "maxBlocks must be a power of 2");

  bool enabled = false;

  EthernetClient *client = nullptr;

	audio_block_t* volatile queue[maxBlocks][2] = {};
	volatile uint8_t tail = 0;
	uint8_t head = 0;

  static constexpr int blockSize = AUDIO_BLOCK_SAMPLES * sizeof(int16_t) * 2;

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
