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
      Serial.println("AudioInputEther enabled");
      enabled = true;
    } else {
      enabled = false;
      Serial.println("AudioInputEther disabled");
    }
  }
	void end() {
    Serial.println("AudioInputEther disabled");
    enabled = false;
  }

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
      //Serial.println("AudioInputEther trying to connect");
      _client->stop();
      return _client->connect(serverIP, port);
    }
  }
  //void stop() { _client->stop(); }
  void setNoDelay() { _client->setNoDelay(true); }

  // send queued data to stream
  void update() override {
    audio_block_t *blockL, *blockR;

    if(tail == head) return; // nothing to stream

    // we have blocks to stream
    blockL = queue[tail][0];
    blockR = queue[tail][1];
    tail = (tail + 1) % maxBlocks;
    transmit(blockL, 0);
    transmit(blockR, 1);
    release(blockL);
    release(blockR);
  }

  // read Ethernet data into queue
  void read() {
    audio_block_t *blockL, *blockR;
    int h, n;
    TOGGLEPROFILEPIN(PROFILER_DECODE_FT8);

    if(!enabled)
    {
      uint8_t dump[512];
      // empty Ethernet buffer
      while(_client->available() > blockSize) {
        TOGGLEPROFILEPIN(PROFILER_PROCESS_FRAME);
        _client->read(dump, blockSize);
      }
      RESETPROFILEPIN(PROFILER_PROCESS_FRAME);
      RESETPROFILEPIN(PROFILER_DECODE_FT8);
      return;
    }

    //int avail = _client->available();
    h = (head + 1) % maxBlocks;
    //if((avail >= blockSize) && (h != tail)) {
    //while((avail >= blockSize) && (h != tail)) {
    //if((_client->available() >= blockSize) && (h != tail)) {
    while((_client->available() > blockSize) && (h != tail)) {
    //while((_client->available() >= blockSize * 2) && (h != tail)) {
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
      n = _client->read((uint8_t *)blockL->data, blockSize / 2);
      n += _client->read((uint8_t *)blockR->data, blockSize / 2);
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
          //Serial.println("dropping block");
        }
        queue[head][0] = blockL;
        queue[head][1] = blockR;
        head = h;
      }
      //avail -= blockSize;
    }
    RESETPROFILEPIN(PROFILER_PROCESS_FRAME);
    RESETPROFILEPIN(PROFILER_DECODE_FT8);
    RESETPROFILEPIN(PROFILER_FT8_REMOTE_RX);
  }

  explicit operator bool() {
    return _client ? 1 : 0;
  }

private:
	//static constexpr int maxBlocks = 50;
	static constexpr int maxBlocks = 200;
  bool enabled = false;

  EthernetClient *_client = nullptr;

  // Network configuration for the Client
  const IPAddress clientIP{192, 168, 1, 101}; // Must be different from Server
  const IPAddress subnet{255, 255, 255, 0};
  const IPAddress gateway{192, 168, 1, 1};

  // Target Server Configuration
  const IPAddress serverIP{192, 168, 1, 100};
  const uint16_t port = 8023;

	audio_block_t * volatile queue[maxBlocks][2];
	volatile uint8_t head = 0;
	volatile uint8_t tail = 0;

  static constexpr int blockSize = AUDIO_BLOCK_SAMPLES * sizeof(int16_t) * 2;
};
