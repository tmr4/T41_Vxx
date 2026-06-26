
#include "ethernetQueue.h"

#include "SDT.h"

#include "audioInOutQueue.h"

#include "debug.h"

//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

//-------------------------------------------------------------------------------------------------------------
// Code
//-------------------------------------------------------------------------------------------------------------

// *** this is called from an interrupt, it can't touch QNEthernet objects ***
// *** only modifies head! ***
bool EthernetQueue::pushBlocks(audio_block_t* blockL, audio_block_t* blockR) {
  if(isQueueFull()) return false; // queue is full

  queue[head][0] = blockL;
  queue[head][1] = blockR;
  head = (head + 1) & bufferMask;

  return true;
}

// *** this is called from an interrupt, it can't touch QNEthernet objects ***
// *** only modifies tail! ***
bool EthernetQueue::popBlocks(audio_block_t*& blockL, audio_block_t*& blockR) {
  if(head == tail) return false; // queue is empty, nothing to stream

  blockL = queue[tail][0];
  blockR = queue[tail][1];
  if(!blockL || !blockR) {
    // *** this should never happen ***
    release(blockL, blockR);
    return false;
  }
  queue[tail][0] = nullptr;
  queue[tail][1] = nullptr;
  tail = (tail + 1) & bufferMask;

  return true;
}

// write queue data out to UDP
void EthernetQueue::writeFromQueue() {
  if(enabled) {
    audio_block_t *blockL, *blockR;

    SETPROFILEPIN(PROFILER_ENTRY);

    if(isQueueFull()) {
      clear();
    }

    // write while queue has data
    while(head != tail) {
      // take ownership of blocks
      blockL = queue[tail][0];
      blockR = queue[tail][1];
      queue[tail][0] = nullptr;
      queue[tail][1] = nullptr;
      tail = (tail + 1) & bufferMask;

      if(!blockL || !blockR) break; // normally this just duplicates te h!=tail check

      SETPROFILEPIN(PROFILER_RX_TX);
#if RADIO_ROLE == 0
      if(udpClient.beginPacket(clientIP, dataPort)) {
#else
      if(udpClient.beginPacket(serverIP, dataPort)) {
#endif
        udpClient.write((uint8_t*)blockL->data, blockSize / 2);
        udpClient.write((uint8_t*)blockR->data, blockSize / 2);
        //udpClient.write((uint8_t*)&sequenceCounter, sizeof(uint32_t));
        udpClient.endPacket();
        //++sequenceCounter;
      }

      release(blockL, blockR);
    }

    RESETPROFILEPIN(PROFILER_ENTRY);
    RESETPROFILEPIN(PROFILER_RX_TX);
  }
}

// read UDP data into queue
void EthernetQueue::readToQueue() {
  if(enabled) {
    audio_block_t *blockL, *blockR;
    int available = udpClient.parsePacket();
    int n;

    SETPROFILEPIN(PROFILER_ENTRY);

    if(isQueueFull()) {
      clear();
    }

    // read while data is available
    while(available >= 0) {
      // IQ packet is 256-bytes I, 256-bytes Q, uint32 sequence (if enabled)
      //if(available == blockSize + sizeof(uint32_t)) {
      if(available == blockSize) {
        // we have sufficient data to queue
        allocate(blockL, blockR);
        if(!blockL || !blockR) {
          release(blockL, blockR);
          break;
        }

        SETPROFILEPIN(PROFILER_RX_TX);
        n = udpClient.read((uint8_t *)blockL->data, blockSize / 2);
        n += udpClient.read((uint8_t *)blockR->data, blockSize / 2);
        //udpClient.read((uint8_t *)&sequenceCounter, sizeof(uint32_t));

        //if(sequenceCounter != (lastSequenceCounter + 1)) {
        //  // reset
        //  expectedSequenceCounter = sequenceCounter;
        //}
        //lastSequenceCounter = sequenceCounter;
        //++expectedSequenceCounter;

        if(n < blockSize) {
          // read error
          release(blockL, blockR);
          break;
        }

        queue[head][0] = blockL;
        queue[head][1] = blockR;
        head = (head + 1) & bufferMask;
      } else {
        udpClient.flush();
      }

      available = udpClient.parsePacket();
    }
  } else {
    while(udpClient.parsePacket() >= 0) {
      udpClient.flush();
    }
  }

  RESETPROFILEPIN(PROFILER_ENTRY);
  RESETPROFILEPIN(PROFILER_RX_TX);
}

void EthernetQueue::clear() {
  audio_block_t *blockL, *blockR;

  noInterrupts();
  for(size_t i = 0; i < maxBlocks; i++) {
    blockL = queue[i][0];
    blockR = queue[i][1];
    release(blockL, blockR);
    queue[i][0] = nullptr;
    queue[i][1] = nullptr;
  }
  head = tail = 0;
  interrupts();
}

void EthernetQueue::allocate(audio_block_t*& blockL, audio_block_t*& blockR) {
  audioStream->allocateBlocks(blockL, blockR);
}

void EthernetQueue::release(audio_block_t* blockL, audio_block_t* blockR) {
  audioStream->releaseBlocks(blockL, blockR);
}
