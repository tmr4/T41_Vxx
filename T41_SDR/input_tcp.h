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
	void end() { enabled = false; }

  // send queued data to stream
  // *** this is called from an interrupt, it can't touch QNEthernet objects ***
  void update() override {
    audio_block_t *blockL, *blockR;

    if(tail == head) return; // nothing to stream

    if(enabled) {
      // we have blocks to stream
      blockL = queue[tail][0];
      blockR = queue[tail][1];
      tail = (tail + 1) % maxBlocks;
      transmit(blockL, 0);
      transmit(blockR, 1);
      release(blockL);
      release(blockR);
    }
  }

  // read Ethernet data into queue
  void read() {
    audio_block_t *blockL, *blockR;
    int h, n;

    if(client) {
      Ethernet.loop();

      if(enabled) {
        TOGGLEPROFILEPIN(PROFILER_DECODE_FT8);
        //int avail = client->available();
        h = (head + 1) % maxBlocks;
        while((client->available() >= blockSize) && (h != tail)) {
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
            h = (head + 1) % maxBlocks;
            if(h == tail) {
              // buffer full, drop oldest block
              audio_block_t *oldestL = queue[tail][0];
              audio_block_t *oldestR = queue[tail][1];
              if(oldestL) release(oldestL);
              if(oldestR) release(oldestR);
              tail = (tail + 1) % maxBlocks;
              TOGGLEPROFILEPIN(PROFILER_FT8_CAT_TX);
            }
            queue[head][0] = blockL;
            queue[head][1] = blockR;
            head = h;
          }
          //avail -= blockSize;
        }
      } else {
        uint8_t dump;
        // empty Ethernet buffer
        while(client->available()) {
          TOGGLEPROFILEPIN(PROFILER_PROCESS_FRAME);
          client->read(&dump, 1);
        }

        RESETPROFILEPIN(PROFILER_PROCESS_FRAME);
        RESETPROFILEPIN(PROFILER_DECODE_FT8);
        RESETPROFILEPIN(PROFILER_FT8_REMOTE_RX);
        RESETPROFILEPIN(PROFILER_FT8_CAT_TX);
      }
    }
  }

private:
	static constexpr int maxBlocks = 50;
	//static constexpr int maxBlocks = 200;
  bool enabled = false;

  EthernetClient *client;

	audio_block_t * volatile queue[maxBlocks][2];
	volatile uint8_t head = 0;
	volatile uint8_t tail = 0;

  static constexpr int blockSize = AUDIO_BLOCK_SAMPLES * sizeof(int16_t) * 2;

  friend class ConnectManager;
};
