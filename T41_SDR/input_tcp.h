#pragma once

/*
 AudioInputTCP - Streams 2-channels from set TCP port to connected AudioStream objects

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
#include "telemetry.h"

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
    clear();
  }

  void setClient(EthernetClient* _client) {
    if(!_client) clear();
    client = _client;
  }

  // send queued data to stream
  // *** this is called from an interrupt, it can't touch QNEthernet objects ***
  // *** only touches tail! ***
  void update() override {
    audio_block_t *blockL, *blockR;

    if(head == tail) return; // queue is empty, nothing to stream

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
  // *** EthernetClient::available and read call Ethernet.loop() through
  //     getStateAndLoopOrClose so a call isn't needed here ***
  void read() {
    if(client) {
      if(enabled) {
        audio_block_t *blockL, *blockR;
        int available = client->available();
        int n;
        size_t t;
        bool bFull;

        TOGGLEPROFILEPIN(PROFILER_DECODE_FT8);

        // lock stuff for this loop
        noInterrupts();
        t = tail;
        bFull = bufferFull();
        interrupts();

        if(0) {
          // forced a TCP stall for telemetry testing
          static unsigned long lastStall = millis();
          unsigned long now = millis();
          static bool forceStall = false;
          // force a TCP stall every 60s
          if(now - lastStall > 60000) {
            forceStall = true;
            Serial.println("Forcing stall...");
            lastStall = now;
          }
          // ignore ethernet for 0.74s (causes a ~1.5-1.7s stall as system recovers)
          if(forceStall) {
            if(now - lastStall < 740) {
              return;
            } else {
              forceStall = false;
            }
          }
        }

        // *** buffer full check (usually happens on system glitch) ***
        // I've tested just dropping the oldest block and adding the new one.
        // The system recovers after the glitch, but spends sometime doing
        // the swap with no real gain. The damage (audio artifact) is already done.
        // Better is to just clear the entire buffer and allow the system to recover
        // faster instead of trying to force through old data. Setting a flag
        // to note buffer was fully during an update allows clear() to run from
        // an interrupt.
        if(bFull) {
          telemetry.bufferClearEvent(head, t, available);
          clear();
          t = 0;
        } else {
          telemetry.preLoopCheck(head, t, available);
        }

        while(available >= blockSize) {
          telemetry.inLoopCheck(head, t, available);

          TOGGLEPROFILEPIN(PROFILER_PROCESS_FRAME);

          // we have sufficient data to queue
          blockL = allocate();
          blockR = allocate();

          if(!blockL || !blockR || bFull) {
            if(blockL) release(blockL);
            if(blockR) release(blockR);

            RESETPROFILEPIN(PROFILER_PROCESS_FRAME);
            RESETPROFILEPIN(PROFILER_DECODE_FT8);
            return;
          }

          n = client->read((uint8_t *)blockL->data, blockSize / 2);
          n += client->read((uint8_t *)blockR->data, blockSize / 2);
          if(n < blockSize) {
            // read error
            release(blockL);
            release(blockR);
            break;
          }
          TOGGLEPROFILEPIN(PROFILER_FT8_REMOTE_RX);
          queue[head][0] = blockL;
          queue[head][1] = blockR;
          head = (head + 1) & bufferMask;
          available -= 512;
        }
      } else {
        uint8_t dump[512];
        // empty Ethernet buffer
        while(client->available() >= 512) {
          TOGGLEPROFILEPIN(PROFILER_FT8_CAT_TX);
          client->read(dump, 512);
        }
        // get the last bit
        while(client->available()) {
          TOGGLEPROFILEPIN(PROFILER_FT8_CAT_TX);
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
	volatile size_t tail = 0;
	size_t head = 0;

  bool bufferClearEvent = false;

  static constexpr int blockSize = AUDIO_BLOCK_SAMPLES * sizeof(int16_t) * 2;

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
