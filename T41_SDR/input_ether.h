#pragma once

/*
 AudioInputEther - Streams 2-channels from selected Ethernet object to connect output objects

Remote timing (w/ T41 standard input, Auto NF):
  * USB serial input, 512-bytes total, written directly to 2-channel output (not buffered)
    * { L-channel block, R-channel block } or { 256-bytes left channel, 256-bytes right channel } input to
      { 256-bytes left channel } and { 256-bytes right channel }
  * update() run every 667us (2.9ms /44.1kHz * 192kHz)
  * ~3.6us to read 512-bytes from USB serial
  * ~1.4ms to process the 16 blocks of data required to form a frame for display
    * The remote take half as long to process data due to the shorter time required to
      read data from USB serial as the T41 takes to write the same amount of data to
      USB Host serial.
  * Time to complete one update of display (frame):
    * ~75ms or ~13.4 frames/sec
  * Notes:
    * The T41 and remote prepare to render the next frame immediately after completing the
      previous frame. The different frame rates mean that the two units aren't rendering the same
      data slices at any given time.
    * The T41 and remote are running at different clock rates, 528MHz for the T41 for Teensy
      longevity and 600MHz on the remote due to AP instability at 528MHz (Teensy chip voltage issue)

 Works with AudioOutputEther

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

class AudioInputEther : public AudioStream {
public:
  AudioInputEther() : AudioStream(0, nullptr) {}

  void init(EthernetClient *client) {
    if(client == nullptr) return;

    _client = client;

    // Configure static IP
    InitEthernet(clientIP, subnet, gateway);
  }
	void begin() {
    if(_client->connected()) {
      enabled = true;
    } else {
      enabled = false;
    }
  }
	void end() { enabled = false;	}

  uint8_t connected() {
    if(!_client) {
      return 0;
    } else {
      return _client->connected();
    }
  }
  int connect() {
    if(!_client) {
      return 0;
    } else {
      return _client->connect(serverIP, port);
    }
  }

  void update() override {
    audio_block_t *blockL, *blockR;
    int n;

    TOGGLEPROFILEPIN(PROFILER_DECODE_FT8);
    if(!enabled) {
      uint8_t dump;
      // empty USB buffer
      while(_client->available()) _client->read(&dump, 1);
      RESETPROFILEPIN(PROFILER_DECODE_FT8);
      return;
    }

    if(_client->available() < blockSize) {
      RESETPROFILEPIN(PROFILER_DECODE_FT8);
      return;
    }

    TOGGLEPROFILEPIN(PROFILER_PROCESS_FRAME);
    blockL = allocate();
    blockR = allocate();
    if(!blockL || !blockR) {
      if(blockL) release(blockL);
      if(blockR) release(blockR);
      RESETPROFILEPIN(PROFILER_PROCESS_FRAME);
      RESETPROFILEPIN(PROFILER_DECODE_FT8);
      return;
    }

    n = _client->read((uint8_t *)blockL->data, blockSize / 2);
    n += _client->read((uint8_t *)blockR->data, blockSize / 2);
    if(n == blockSize) {
      TOGGLEPROFILEPIN(PROFILER_FT8_REMOTE_RX);
      transmit(blockL, 0);
      transmit(blockR, 1);
    }

    release(blockL);
    release(blockR);
    RESETPROFILEPIN(PROFILER_PROCESS_FRAME);
    RESETPROFILEPIN(PROFILER_DECODE_FT8);
    RESETPROFILEPIN(PROFILER_FT8_REMOTE_RX);
  }

private:
  bool enabled = false;

  EthernetClient *_client = nullptr;

  static constexpr int blockSize = AUDIO_BLOCK_SAMPLES * sizeof(int16_t) * 2;

  // Network configuration for the Client
  const IPAddress clientIP{192, 168, 1, 101}; // Must be different from Server
  const IPAddress subnet{255, 255, 255, 0};
  const IPAddress gateway{192, 168, 1, 1};

  // Target Server Configuration
  const IPAddress serverIP{192, 168, 1, 100};
  const uint16_t port = 8023;
};
