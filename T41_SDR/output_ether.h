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
    _server->begin();
    _client = _server->accept();
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
      } else {
        enabled = false;
      }
    }
  }
	void end() {
    if(_client) _client.stop();
    //_client = nullptr;
    enabled = false;
  }

  void update() override {
    audio_block_t *blockL, *blockR;

    TOGGLEPROFILEPIN(PROFILER_DECODE_FT8);
    blockL = receiveReadOnly(0);
    blockR = receiveReadOnly(1);

    if(!enabled || !blockL || !blockR) {
      if(blockL) release(blockL);
      if(blockR) release(blockR);
      RESETPROFILEPIN(PROFILER_DECODE_FT8);
      return;
    }

    TOGGLEPROFILEPIN(PROFILER_PROCESS_FRAME);
    if (_client && _client.connected()) {
      if(_client.availableForWrite() < blockSize) {
        release(blockL);
        release(blockR);
        RESETPROFILEPIN(PROFILER_PROCESS_FRAME);
        RESETPROFILEPIN(PROFILER_DECODE_FT8);
        return;
      }

      TOGGLEPROFILEPIN(PROFILER_FT8_CAT_TX);
      _client.write((uint8_t *)blockL->data, blockSize / 2);
      _client.write((uint8_t *)blockR->data, blockSize / 2);
    }

    release(blockL);
    release(blockR);
    RESETPROFILEPIN(PROFILER_PROCESS_FRAME);
    RESETPROFILEPIN(PROFILER_DECODE_FT8);
    RESETPROFILEPIN(PROFILER_FT8_CAT_TX);
  }

private:
  bool enabled = false;

  // Network configuration for the Server
  const IPAddress serverIP{192, 168, 1, 100};
  const IPAddress subnet{255, 255, 255, 0};
  const IPAddress gateway{192, 168, 1, 1};

  const uint16_t port = 8023;
  EthernetServer *_server = nullptr;
  EthernetClient _client; // this persistent instance keeps the connection alive

  static constexpr int blockSize = AUDIO_BLOCK_SAMPLES * sizeof(int16_t) * 2;
  audio_block_t *inputQueueArray[2];
};
