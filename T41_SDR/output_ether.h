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

  void init(EthernetServer *server) {
    if(server == nullptr) return;

    _server = server;

    // Configure static IP
    InitEthernet(serverIP, subnet, gateway);
    //_server->begin();
    _server->beginWithReuse();
  }
  uint8_t connected() {
    if(_client) {
      return _client.connected();
    } else {
      return 0;
    }
  }
  int connect() {
    //_client = _server->accept();
    EthernetClient newB = _server->accept();
    if(newB) {
      _client.stop();
      _client = newB;
      Serial.println("AudioOutputEther accepted client");
      return 1;
    } else {
      return 0;
    }
  }
	void begin() {
    if(!_server) {
      enabled = false;
    } else {
      //_client = _server->available();
      //_client = _server->available();
      //_client = _server->accept();
      if(_client && _client.connected()) {
        enabled = true;
        Serial.println("AudioOutputEther enabled");
      } else {
        enabled = false;
      }
    }
  }
	void end() {
    //if(_client) _client.stop();
    //_client = nullptr;
    enabled = false;
  }

  // store stream in queue
  void update() override {
    audio_block_t *blockL, *blockR;
    int h;

    TOGGLEPROFILEPIN(PROFILER_DECODE_FT8);
    blockL = receiveReadOnly(0);
    blockR = receiveReadOnly(1);

    h = (head + 1) % maxBlocks;
    if(!enabled || (h == tail) || !blockL || !blockR) {
      if(blockL) release(blockL);
      if(blockR) release(blockR);
      Serial.println("dropping block");
      RESETPROFILEPIN(PROFILER_DECODE_FT8);
      return;
    }

    // we're enabled an have blocks to queue
    TOGGLEPROFILEPIN(PROFILER_PROCESS_FRAME);
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

    RESETPROFILEPIN(PROFILER_PROCESS_FRAME);
    RESETPROFILEPIN(PROFILER_DECODE_FT8);
    RESETPROFILEPIN(PROFILER_FT8_CAT_TX);
  }

  // write queue data out to Ethernet
  void write() {
    TOGGLEPROFILEPIN(PROFILER_DECODE_FT8);
    if(tail == head) {
      return; // nothing to write
    }

    if(_client && _client.connected()) {
      TOGGLEPROFILEPIN(PROFILER_PROCESS_FRAME);
      int avail = _client.availableForWrite();
      audio_block_t *blockL, *blockR;
      //Serial.println(avail);
      while((avail >= blockSize) && (tail != head)) {
        //Serial.println(avail);
        TOGGLEPROFILEPIN(PROFILER_FT8_CAT_TX);
        blockL = queue[tail][0];
        blockR = queue[tail][1];

        _client.write((uint8_t *)blockL->data, blockSize / 2);
        _client.write((uint8_t *)blockR->data, blockSize / 2);

        release(blockL);
        release(blockR);
        avail -= blockSize;
        tail = (tail + 1) % maxBlocks;
      }
    }
    RESETPROFILEPIN(PROFILER_PROCESS_FRAME);
    RESETPROFILEPIN(PROFILER_DECODE_FT8);
    RESETPROFILEPIN(PROFILER_FT8_CAT_TX);
  }

private:
	static constexpr int maxBlocks = 50;
  bool enabled = false;

  // Network configuration for the Server
  const IPAddress serverIP{192, 168, 1, 100};
  const IPAddress subnet{255, 255, 255, 0};
  const IPAddress gateway{192, 168, 1, 1};

  const uint16_t port = 8023;
  EthernetServer *_server = nullptr;
  EthernetClient _client; // this persistent instance keeps the connection alive

	audio_block_t * volatile queue[maxBlocks][2];
	volatile uint8_t head = 0;
	volatile uint8_t tail = 0;

  static constexpr int blockSize = AUDIO_BLOCK_SAMPLES * sizeof(int16_t) * 2;
  audio_block_t *inputQueueArray[2];
};
