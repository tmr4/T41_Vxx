#pragma once

/*

EthernetBridgeServer - manages TCP server connection

EthernetBridgeClient - manages TCP client connection

EthernetBridgeQueue - Queues 2-channels to/from related AudioStreams and a UDP data port

Data Structure:
  * 2-channel input, 512-bytes total, buffered and then written to UDP data port
    * { L-channel block, R-channel block } or { 256-bytes left channel, 256-bytes right channel }

 Works with AudioInputFromQueue and AudioOutputToQueue

*/

#include <QNEthernet.h>
using namespace qindesign::network;

#include "connectBase.h"
#include "tcpManager.h"

//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

class AudioOutputToQueue;

class EthernetBridgeServer : public ConnectBase {
 public:
  EthernetBridgeServer(uint16_t cPort = 8000) : cmdPort(cPort), tcpServer(cmdPort) {}

  void init() override { tcpServer.begin(); }

	void begin() override { enabled = true; };
	void end() override { enabled = false; }

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

private:
  bool enabled = false;

  uint16_t cmdPort;

  TCPServer tcpServer; // CAT command channel
};

class EthernetBridgeClient : public ConnectBase {
public:
  EthernetBridgeClient(uint16_t cPort = 8000) : cmdPort(cPort), tcpClient(cmdPort)  {}

  void init() override { tcpClient.begin(); }

	void begin() override { enabled = true; };
	void end() override { enabled = false; }

  bool linkStatus() override { return Ethernet.linkState(); }

  bool connect() override {
    tcpClient.connect();

    if(tcpClient.connected()) {
      enabled = true;
    } else {
      enabled = false;
    }

    return enabled;
  }

  void disconnect() override {
    tcpClient.disconnect();
    end();
  }

  bool connected() override { return tcpClient.connected(); }

  Stream* getCommandStream() override { return tcpClient.getClient(); }
  ConnectMode getConnectionType() override { return CONNECT_ETHERNET; }

private:
  bool enabled = false;

  uint16_t cmdPort;

  TCPClient tcpClient; // CAT command channel
};

class EthernetBridgeQueue {
 public:
  EthernetBridgeQueue(uint16_t dPort = 8001) : dataPort(dPort) {}

  void init(AudioOutputToQueue* s) {
    if(s) {
      audioStream = s;
      udpClient.begin(dataPort);
      enabled = true;
    }
  }

	void begin() { if(audioStream) enabled = true; };
	void end() {
    enabled = false;
    clear();
  }

  bool pushBlocks(audio_block_t* blockL, audio_block_t* blockR);
  bool popBlocks(audio_block_t*& blockL, audio_block_t*& blockR);

  void writeFromQueue();
  void readToQueue();

  bool isQueueFull() { return ((head + 1) & bufferMask) == tail; }
  bool isQueueEmpty() { return head == tail; }

private:
  bool enabled = false;

  uint16_t dataPort;
  EthernetUDP udpClient{8}; // IQ data channel
  const IPAddress clientIP{192, 168, 1, 101};

  uint32_t sequenceCounter = 0;
  uint32_t lastSequenceCounter = 0;
  uint32_t expectedSequenceCounter = 0;

  AudioOutputToQueue* audioStream;

  static constexpr int blockSize = AUDIO_BLOCK_SAMPLES * sizeof(int16_t) * 2;
	static constexpr size_t maxBlocks = 64; // *** must be power of 2 ***
	static constexpr size_t bufferMask = maxBlocks - 1;
  static_assert((maxBlocks & (maxBlocks - 1)) == 0, "maxBlocks must be a power of 2");

	audio_block_t* volatile queue[maxBlocks][2] = {};
	volatile size_t head = 0;
  volatile size_t tail = 0;

  void clear();

  void allocate(audio_block_t*& blockL, audio_block_t*& blockR);
  void release(audio_block_t* blockL, audio_block_t* blockR);
};
