#pragma once

/*
 AudioOutputEther - Streams 2-channels from connected input objects to specified Ethernet object

T41 timing (w/ T41 standard input, Auto NF):
  * 2-channel input, 512-bytes total, written directly to USB Host serial (not buffered)
    * { L-channel block, R-channel block } or { 256-bytes left channel, 256-bytes right channel }
  * update() run every 667us (2.9ms /44.1kHz * 192kHz)
  * ~205us to write 512-bytes to USB Host serial
  * ~2.8ms to process the 16 blocks of data required to form a frame for display
    * T41 take twice as long to process data due to the longer time required to
      write to USB Host serial than the remote needs to read the same amount of
      data from USB serial.
  * This time adds to the time to complete one update of display (frame):
    * ~150ms or ~6.7 frames/sec with remote attached
    * ~96ms w/o remote ~10.4 frames/sec
  * Notes:
    * The T41 and remote prepare to render the next frame immediately after completing the
      previous frame. The different frame rates mean that the two units aren't rendering the same
      data slices at any given time.
    * The T41 and remote are running at different clock rates, 528MHz for the T41 for Teensy
      longevity and 600MHz on the remote due to AP instability at 528MHz (Teensy chip voltage issue)

 Works with AudioInputEther

 *** This object could be made more robust with a buffer and syncing but
     early testing hasn't shown a need for this ***

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

class AudioOutputEther : public AudioStream {
public:
  AudioOutputEther() : AudioStream(2, inputQueueArray) {}

  void begin() {
    if(!enabled) {
      InitEthernet(serverIP, subnet, gateway); // configure static IP
      _server.begin(port);
      _client.setConnectionTimeoutEnabled(false);
      _client.setNoDelay(true);
      enabled = true;
    }
  }
	void end() { enabled = false; }


  // store stream in queue
  void update() override {
    audio_block_t *blockL, *blockR;
    int h;

    blockL = receiveReadOnly(0);
    blockR = receiveReadOnly(1);

    h = (head + 1) % maxBlocks;
    if(!connected || !enabled || (h == tail) || !blockL || !blockR) {
      if(blockL) release(blockL);
      if(blockR) release(blockR);
      return;
    }

    // we're enabled an have blocks to queue
    //h = (head + 1) % maxBlocks;
    //if(h == tail) {
    //  // buffer full, drop oldest block
    //  audio_block_t *oldestL = queue[tail][0];
    //  audio_block_t *oldestR = queue[tail][1];
    //  if(oldestL) release(oldestL);
    //  if(oldestR) release(oldestR);
    //  tail = (tail + 1) % maxBlocks;
    //  Serial.println("dropping block");
    //}
    queue[head][0] = blockL;
    queue[head][1] = blockR;
    head = h;
  }

  // write queue data out to Ethernet
  void write() {
    if(enabled) {
      TOGGLEPROFILEPIN(PROFILER_DECODE_FT8);

      Ethernet.loop();

      // stop dead or broken connections
      if(!_client.connected()) {
        _client.stop();

        // accept incoming connections
        _client = _server.accept();
        if(_client) {
          _client.setConnectionTimeoutEnabled(false);
          _client.setNoDelay(true);
          connected = true;
        } else {
          connected = false;
        }
      }

      if(tail == head) {
        RESETPROFILEPIN(PROFILER_DECODE_FT8);
        return; // nothing to write
      }

      if(_client) {
        TOGGLEPROFILEPIN(PROFILER_PROCESS_FRAME);
        audio_block_t *blockL, *blockR;
        while((_client.availableForWrite() > blockSize) && (tail != head)) {
          TOGGLEPROFILEPIN(PROFILER_FT8_CAT_TX);
          blockL = queue[tail][0];
          blockR = queue[tail][1];

          _client.write((uint8_t *)blockL->data, blockSize / 2);
          _client.write((uint8_t *)blockR->data, blockSize / 2);
          //_client.writeFully((uint8_t *)blockL->data, blockSize / 2);
          //_client.writeFully((uint8_t *)blockR->data, blockSize / 2);

          release(blockL);
          release(blockR);
          tail = (tail + 1) % maxBlocks;
        }
        _client.flush();
      }
      RESETPROFILEPIN(PROFILER_PROCESS_FRAME);
      RESETPROFILEPIN(PROFILER_DECODE_FT8);
      RESETPROFILEPIN(PROFILER_FT8_CAT_TX);
    }
  }

private:
	static constexpr int maxBlocks = 50;
  bool enabled = false;
  bool connected = false;

  // Network configuration for the Server
  const IPAddress serverIP{192, 168, 1, 100};
  const IPAddress subnet{255, 255, 255, 0};
  const IPAddress gateway{192, 168, 1, 1};

  const uint16_t port = 8023;
  EthernetServer _server;
  EthernetClient _client; // this persistent instance keeps the connection alive

	audio_block_t * volatile queue[maxBlocks][2];
	volatile uint8_t head = 0;
	volatile uint8_t tail = 0;

  static constexpr int blockSize = AUDIO_BLOCK_SAMPLES * sizeof(int16_t) * 2;
  audio_block_t *inputQueueArray[2];
};
