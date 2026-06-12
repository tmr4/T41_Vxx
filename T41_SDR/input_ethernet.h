#pragma once

/*
 AudioInputUDP - Streams 2-channels from set UDP port to connected AudioStream objects

 Works with AudioOutputUDP

*/

#if RADIO_ROLE == 2

#include <AudioStream.h>
#include <QNEthernet.h>
using namespace qindesign::network;

#include "connectBase.h"
#include "connectManager.h"
#include "tcpManager.h"

#include "debug.h"

//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

class AudioInputUDP : public AudioStream, public ConnectBuffered<size_t, volatile size_t> {
public:
  AudioInputUDP(uint16_t cPort = 8000, uint16_t dPort = 8001) : AudioStream(0, nullptr),
    cmdPort(cPort), dataPort(dPort) {}

	void end() override {
    enabled = false;
    clear();
  }

  bool linkStatus() override { return Ethernet.linkState(); }

  bool connect() override {
    tcpClient.connect();

    if(tcpClient.connected()) {
      udpClient.begin(dataPort);
      enabled = true;
      return true;
    }

    return false;
  }

  void disconnect() override {
    tcpClient.disconnect();
    end();
  }

  bool connected() override { return tcpClient.connected(); }

  Stream* getCommandStream() override { return tcpClient.getClient(); }
  ConnectMode getConnectionType() override { return CONNECT_ETHERNET; }

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

  // read UDP data into queue
  void readToQueue() override {
    if(udpClient) {
      if(enabled) {
        audio_block_t *blockL, *blockR;
        int available = udpClient->parsePacket();
        int n;
        bool bFull;

        SETPROFILEPIN(PROFILER_ENTRY);

        // lock stuff for this loop
        noInterrupts();
        bFull = bufferFull();
        interrupts();

        if(bFull) {
          //Serial.println("buffer full in AudioInputUDP");
          clear();
        }

        while(available >= 0) {
          // IQ packet is 256-bytes I, 256-bytes Q, uint32 sequence
          if(available == blockSize + sizeof(uint32_t)) {
            // we have sufficient data to queue
            blockL = allocate();
            blockR = allocate();

            if(!blockL || !blockR) {
              if(blockL) release(blockL);
              if(blockR) release(blockR);
              break;
            }

            SETPROFILEPIN(PROFILER_RX_TX);
            n = udpClient->read((uint8_t *)blockL->data, blockSize / 2);
            n += udpClient->read((uint8_t *)blockR->data, blockSize / 2);
            udpClient->read((uint8_t *)&sequenceCounter, sizeof(uint32_t));

            if(sequenceCounter != (lastSequenceCounter + 1)) {
              //Serial.printf("%u dropped packets in AudioInputUDP\n", sequenceCounter - expectedSequenceCounter);

              // reset
              expectedSequenceCounter = sequenceCounter;
            }
            lastSequenceCounter = sequenceCounter;
            ++expectedSequenceCounter;

            if(n < blockSize) {
              // read error
              release(blockL);
              release(blockR);
              //Serial.println("incomplete read in AudioInputUDP");
              break;
            }

            queue[head][0] = blockL;
            queue[head][1] = blockR;
            head = (head + 1) & bufferMask;
          } else {
            //Serial.println("incomplete packet in AudioInputUDP");
            udpClient->flush();
          }

          available = udpClient->parsePacket();
        }
      } else {
        while(udpClient->parsePacket() >= 0) {
          udpClient->flush();
        }
      }
    }

    RESETPROFILEPIN(PROFILER_ENTRY);
    RESETPROFILEPIN(PROFILER_RX_TX);
  }

private:
  TCPClient tcpClient{cmdPort}; // CAT command channel
  EthernetUDP udpClient{32}; // IQ data channel

  uint16_t cmdPort = 0;
  uint16_t dataPort = 0;

  uint32_t sequenceCounter = 0;
  uint32_t lastSequenceCounter = 0;
  uint32_t expectedSequenceCounter = 0;

  static constexpr int blockSize = AUDIO_BLOCK_SAMPLES * sizeof(int16_t) * 2;

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
};

#endif
