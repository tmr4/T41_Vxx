#pragma once

/*
 AudioInputUDP - Streams 2-channels from set UDP port to connected AudioStream objects

 Works with AudioOutputUDP

*/

#include <QNEthernet.h>
using namespace qindesign::network;

#include "connectBase.h"
#include "debug.h"

//-------------------------------------------------------------------------------------------------------------
// Code
//-------------------------------------------------------------------------------------------------------------

class AudioInputUDP : public AudioConnectBuffered<size_t, volatile size_t> {
public:
  AudioInputUDP() : AudioConnectBuffered(0, nullptr) {}

	void end() override {
    enabled = false;
    clear();
  }

  void setClient(EthernetUDP* _client) {
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

  // read UDP data into queue
  void readToQueue() override {
    if(client) {
      if(enabled) {
        audio_block_t *blockL, *blockR;
        int available = client->parsePacket();
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
            n = client->read((uint8_t *)blockL->data, blockSize / 2);
            n += client->read((uint8_t *)blockR->data, blockSize / 2);
            client->read((uint8_t *)&sequenceCounter, sizeof(uint32_t));

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
            client->flush();
          }

          available = client->parsePacket();
        }
      } else {
        while(client->parsePacket() >= 0) {
          client->flush();
        }
      }
    }

    RESETPROFILEPIN(PROFILER_ENTRY);
    RESETPROFILEPIN(PROFILER_RX_TX);
  }

private:
  EthernetUDP *client = nullptr;
  uint32_t sequenceCounter = 0;
  uint32_t lastSequenceCounter = 0;
  uint32_t expectedSequenceCounter = 0;

  static constexpr int blockSize = AUDIO_BLOCK_SAMPLES * sizeof(int16_t) * 2;
};
