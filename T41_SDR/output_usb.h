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
  AudioUSBSender(USBSerial_BigBuffer& serial) : AudioStream(2, inputQueueArray), _serial(serial) {
  //  enabled = true;
  }
  //AudioUSBSender() : AudioStream(2, inputQueueArray) {}

	void begin() {
		enabled = true;
	}
	void end() {
		enabled = false;
	}

  void update() override {
    TOGGLEPROFILEPIN(PROFILER_DECODE_FT8);
    TOGGLEPROFILEPIN(PROFILER_PROCESS_FRAME);
    audio_block_t *blockL = receiveReadOnly(0);
    audio_block_t *blockR = receiveReadOnly(1);

    if(!blockL || !blockR) {
      if(blockL) release(blockL);
      if(blockR) release(blockR);
      RESETPROFILEPIN(PROFILER_PROCESS_FRAME);
      return;
    }

    if(t41.RemoteStatus != REMOTE_CONNECTED) {
      // *** TODO: do other buffer cleanup work ***

      release(blockL);
      release(blockR);
      //RESETPROFILEPIN(PROFILER_PROCESS_FRAME);
      return;
    }

    UsbHostTask();
    //if(enabled)
    {
      if(_serial.availableForWrite() < 512) {
        // *** TODO: buffer data, wait until next time to continue ***
        release(blockL);
        release(blockR);
        RESETPROFILEPIN(PROFILER_PROCESS_FRAME);
        RESETPROFILEPIN(PROFILER_DECODE_FT8);
        RESETPROFILEPIN(PROFILER_FT8_CAT_TX);
        return;
      }
    }

    TOGGLEPROFILEPIN(PROFILER_FT8_CAT_TX);
    /*
    if(blocks >= frameBlocks) {
      alignas(32) uint8_t syncBlock[512];

      memset(syncBlock, 0, sizeof(syncBlock));
      ((uint32_t *)syncBlock)[0] = syncWord;
      ((uint32_t *)syncBlock)[1] = frameCounter++;

      //if(enabled)
      _serial.write(syncBlock, 512);
      blocks = 0;
    }
    */
    //if(enabled)
    {
      _serial.write((uint8_t *)blockL->data, 256);
      _serial.write((uint8_t *)blockR->data, 256);
    }

    blocks++;

    release(blockL);
    release(blockR);
    UsbHostTask();
    RESETPROFILEPIN(PROFILER_PROCESS_FRAME);
    RESETPROFILEPIN(PROFILER_DECODE_FT8);
    RESETPROFILEPIN(PROFILER_FT8_CAT_TX);
  }

private:
  bool enabled = false;
  USBSerial_BigBuffer& _serial;
  //bool enabled = false;

  static constexpr uint32_t syncWord = 0xA55AA55A;
  static constexpr int frameBlocks = 16;

  uint32_t frameCounter = 0;
  uint32_t blocks = 0;

  audio_block_t *inputQueueArray[2];
};
