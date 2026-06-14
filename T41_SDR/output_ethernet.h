#pragma once

/*

AudioOutputEthernet - Streams 2-channels os data from connected AudioStream objects to UDP data port
                      Handles CAT control channel TCP connection

Data Structure:
  * 2-channel input, 512-bytes total, buffered and then written to UDP data port
    * { L-channel block, R-channel block } or { 256-bytes left channel, 256-bytes right channel }

 Works with AudioInputEthernet

*/

#if RADIO_ROLE == 7

#include <AudioStream.h>
#include <QNEthernet.h>
using namespace qindesign::network;

#include "connectBase.h"
#include "tcpManager.h"

#include "debug.h"

//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

struct AudioBufferBase {
  audio_block_t* inputQueueArray[2];
  AudioBufferBase() : inputQueueArray{nullptr, nullptr} {}
};

class AudioOutputEthernet : private AudioBufferBase,
                            public AudioStream,
                            public ConnectBase
{
 public:
  AudioOutputEthernet(uint16_t cPort = 8000, uint16_t dPort = 8001) : AudioStream(2, inputQueueArray),
  cmdPort(cPort), dataPort(dPort), tcpServer(cmdPort) {}

  void init() override {
    tcpServer.begin();
    udpClient.begin(dataPort);
  }

	void begin() override { enabled = true; };
	void end() override {
    enabled = false;
    clear();
  }

  bool linkStatus() override { return Ethernet.linkState(); }

  bool connect() override {
    tcpServer.connect();

    // CAT command channel governs IQ data channel
    if(tcpServer.connected()) {
      enabled = true;
    } else {
      enabled = false;
    }

    return enabled;
  }

  void disconnect() override {
    tcpServer.disconnect();
    end();
  }

  bool connected() override { return tcpServer.connected(); }

  Stream* getCommandStream() override { return tcpServer.getClient(); }
  ConnectMode getConnectionType() override { return CONNECT_ETHERNET; }

  // store input stream in queue
  // *** this is called from an interrupt, it can't touch QNEthernet objects ***
  // *** only touches head! ***
  void update() override {
    audio_block_t* blockL = receiveReadOnly(0);
    audio_block_t* blockR = receiveReadOnly(1);

    if(!enabled || !blockL || !blockR || bufferFull()) {
      if(blockL) release(blockL);
      if(blockR) release(blockR);
      return;
    }

    queue[head][0] = blockL;
    queue[head][1] = blockR;
    head = (head + 1) & bufferMask;
  }

  // write queue data out to UDP
  void writeToQueue() override {
    if(enabled) {
      audio_block_t *blockL, *blockR;
      size_t h;
      bool bFull;

      SETPROFILEPIN(PROFILER_ENTRY);

      // lock stuff for this loop
      noInterrupts();
      h = head;
      bFull = bufferFull();
      interrupts();

      if(bFull) {
        clear();
        h = 0;
      } else {
      }

      // write while queue has data
      while(h != tail) {
        // take ownership of blocks
        blockL = queue[tail][0];
        blockR = queue[tail][1];
        queue[tail][0] = nullptr;
        queue[tail][1] = nullptr;
        tail = (tail + 1) & bufferMask;

        if(!blockL || !blockR) break; // normally this just duplicates te h!=tail check

        SETPROFILEPIN(PROFILER_RX_TX);
        if(udpClient.beginPacket(clientIP, dataPort)) {
          udpClient.write((uint8_t*)blockL->data, blockSize / 2);
          udpClient.write((uint8_t*)blockR->data, blockSize / 2);
          udpClient.write((uint8_t*)&sequenceCounter, sizeof(uint32_t));
          udpClient.endPacket();
          ++sequenceCounter;
        }

        release(blockL);
        release(blockR);
      }

      RESETPROFILEPIN(PROFILER_ENTRY);
      RESETPROFILEPIN(PROFILER_RX_TX);
    }
  }

private:
  bool enabled = false;

  //uint16_t cmdPort = 0;
  //uint16_t dataPort = 0;
  uint16_t cmdPort;
  uint16_t dataPort;

  TCPServer tcpServer; // CAT command channel
  EthernetUDP udpClient; // IQ data channel

  //IPAddress clientIP{0, 0, 0, 0};
  const IPAddress clientIP{192, 168, 1, 101};

  uint32_t sequenceCounter = 0;

  static constexpr int blockSize = AUDIO_BLOCK_SAMPLES * sizeof(int16_t) * 2;
	static constexpr size_t maxBlocks = 64; // *** must be power of 2 ***
	static constexpr size_t bufferMask = maxBlocks - 1;
  static_assert((maxBlocks & (maxBlocks - 1)) == 0, "maxBlocks must be a power of 2");

	audio_block_t* volatile queue[maxBlocks][2] = {};
	volatile size_t head = 0;
  size_t tail = 0;

  bool bufferFull() { return ((head + 1) & bufferMask) == tail; }
  //bool bufferEmpty() { return head == tail; }
  void clear() {
    audio_block_t *blockL, *blockR;

    noInterrupts();
    for(size_t i = 0; i < maxBlocks; i++) {
      blockL = queue[i][0];
      blockR = queue[i][1];
      if(blockL) release(blockL);
      if(blockR) release(blockR);
      queue[i][0] = nullptr;
      queue[i][1] = nullptr;
    }
    head = tail = 0;
    interrupts();
  }

  //audio_block_t *inputQueueArray[2];
};

#endif
