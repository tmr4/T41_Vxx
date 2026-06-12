#pragma once

/*
 AudioOutputHostSerial - Streams 2-channels from connected input objects to specified USB Host serialData object

T41 timing (w/ T41 standard testing input, Auto NF):
  * 2-channel input, 512-bytes total, written directly to USB Host serialData (not buffered)
    * { L-channel block, R-channel block } or { 256-bytes left channel, 256-bytes right channel }
  * update() run every 667us (2.9ms /44.1kHz * 192kHz)
  * ~205us to write 512-bytes to USB Host serialData
  * ~2.8ms to process the 16 blocks of data required to form a frame for display
    * T41 take twice as long to process data due to the longer time required to
      write to USB Host serialData than the remote needs to read the same amount of
      data from USB serialData.
  * This time adds to the time to complete one update of display (frame):
    * ~150ms or ~6.7 frames/sec with remote attached
    * ~96ms w/o remote ~10.4 frames/sec
  * Notes:
    * The T41 and remote prepare to render the next frame immediately after completing the
      previous frame. The different frame rates mean that the two units aren't rendering the same
      data slices at any given time.
    * The T41 and remote are running at different clock rates, 528MHz for the T41 for Teensy
      longevity and 600MHz on the remote due to AP instability at 528MHz (Teensy chip voltage issue)
    * The objects seemlessly handle disconnects in most cases.

 Works with AudioInputSerial

 *** This object could be made more robust with a buffer and syncing but
     early testing hasn't shown a need for this ***

*/

#if RADIO_ROLE == 1

#include <AudioStream.h>
#include <USBHost_t36.h>

#include "connectBase.h"
#include "connectManager.h"

#include "debug.h"

//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

class AudioOutputHostSerial : public AudioStream, public ConnectBase {
public:
  AudioOutputHostSerial(USBHost& _host) : AudioStream(2, inputQueueArray),
    host(&_host), serialCmd(_host, 1), serialData(_host) {}

  void begin() override {
    serialCmd.begin(115200);
    serialData.begin(115200);
    enabled = true;
  }

  bool linkStatus() override { return true; }

  bool connect() override { return true; }

  bool connected() override {
    host->Task();
    //return serialData && serialCmd;
    //return serialCmd ? true : false;
    return true;
  }

  Stream* getCommandStream() override { return &serialCmd; }
  ConnectMode getConnectionType() override { return CONNECT_USB; }

  void update() override {
    audio_block_t *blockL, *blockR;

    blockL = receiveReadOnly(0);
    blockR = receiveReadOnly(1);

    if(!enabled || !blockL || !blockR) {
      if(blockL) release(blockL);
      if(blockR) release(blockR);
      return;
    }

    SETPROFILEPIN(PROFILER_ENTRY);
    host->Task();
    if(serialData.availableForWrite() < blockSize) {
      release(blockL);
      release(blockR);
      RESETPROFILEPIN(PROFILER_ENTRY);
      return;
    }

    TOGGLEPROFILEPIN(PROFILER_RX_TX);
    serialData.write((uint8_t *)blockL->data, blockSize / 2);
    serialData.write((uint8_t *)blockR->data, blockSize / 2);
    host->Task();

    release(blockL);
    release(blockR);
    RESETPROFILEPIN(PROFILER_ENTRY);
    RESETPROFILEPIN(PROFILER_RX_TX);
  }

private:
  USBHost* host = nullptr;
  USBSerial_BigBuffer serialCmd;  // command
  USBSerial_BigBuffer serialData; // data

  static constexpr int blockSize = AUDIO_BLOCK_SAMPLES * sizeof(int16_t) * 2;
  audio_block_t *inputQueueArray[2];
};

#endif
