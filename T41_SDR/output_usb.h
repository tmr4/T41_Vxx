#pragma once

/*

 AudioUSBSender - Teensy AudioStream object

 Sends:
   16 x 512-byte IQ blocks followed by 1 x 512-byte sync block

 IQ block format:
   256 bytes I
   256 bytes Q

 Sync block format:
   uint32_t sync word
   uint32_t frameCounter
   remaining bytes unused
  *** TODO: consider unique block and hash to make more robust ***

 Nonblocking USBSerial_BigBuffer writes

*/

#include <Arduino.h>
#include <AudioStream.h>
#include <USBHost_t36.h>

#include "debug.h"

//-------------------------------------------------------------------------------------------------------------
// Forwards
//-------------------------------------------------------------------------------------------------------------

void SendMsg(const char *msg, int value);
void UsbHostTask();

//-------------------------------------------------------------------------------------------------------------
// Code
//-------------------------------------------------------------------------------------------------------------

//template<typename USBSerial_BigBuffer>
class AudioUSBSender : public AudioStream {
public:
  AudioUSBSender(USBSerial_BigBuffer& serial) : AudioStream(2, inputQueueArray), _serial(serial) {}

  void update(void) override {
    audio_block_t *blockL = receiveReadOnly(0);
    audio_block_t *blockR = receiveReadOnly(1);

    if(!blockL || !blockR) {
      if(blockL) release(blockL);
      if(blockR) release(blockR);
      return;
    }

    UsbHostTask();
    if(!_serial || _serial.availableForWrite() < 512) {
      release(blockL);
      release(blockR);
      return;
    }

    if(blocks >= frameBlocks) {
      alignas(32) uint8_t syncBlock[512];

      memset(syncBlock, 0, sizeof(syncBlock));
      ((uint32_t *)syncBlock)[0] = syncWord;
      ((uint32_t *)syncBlock)[1] = frameCounter++;

      _serial.write(syncBlock, 512);
      blocks = 0;
    }
    TOGGLEPROFILEPIN(PROFILER_FT8_CAT_TX);

    _serial.write((uint8_t *)blockL->data, 256);
    _serial.write((uint8_t *)blockR->data, 256);

    blocks++;

    release(blockL);
    release(blockR);
    UsbHostTask();
  }

private:
  USBSerial_BigBuffer& _serial;

  static constexpr uint32_t syncWord = 0xA55AA55A;
  static constexpr int frameBlocks = 16;

  uint32_t frameCounter = 0;
  uint32_t blocks = 0;

  audio_block_t *inputQueueArray[2];
};
