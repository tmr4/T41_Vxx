#pragma once

/*

AudioInputSerialT - Streams 2-channels pf data from SerialUSB1 to connect output objects
                    Handles CAT control channel over Serial

*** Even though the template class can be configured to work with various serial USB object
    it is currently hardwired to Serial (CAT control) and SerialUSB1 (IQ data) ***

*** Requires Dual Serial ***

Data Structure:
  * USB serial input, 512-bytes total, written directly to 2-channel output (not buffered)
    * { L-channel block, R-channel block } or { 256-bytes left channel, 256-bytes right channel } input to
      { 256-bytes left channel } and { 256-bytes right channel }

Remote timing (w/ T41 standard testing input, Auto NF):
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
    * The objects seemlessly handle disconnects in most cases.

 Works with AudioOutputHostSerial

 *** This object could be made more robust with a buffer and syncing but
     early testing hasn't shown a need for this ***
*/
#if USB_ENABLED

#include <AudioStream.h>

#include "connectBase.h"

#include "debug.h"

//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

extern "C" {
  int usb_serial_read(void *buffer,  uint32_t size);
  int usb_serial2_read(void *buffer, uint32_t size);
  int usb_serial3_read(void *buffer, uint32_t size);

  int usb_serial_available(void);
  int usb_serial2_available(void);
  int usb_serial3_available(void);
}

// *** TODO: consider specializing this to just Dual Serial ***
template< int (*available)(), int (*read)(void *, uint32_t) >
class AudioInputSerialT : public AudioStream, public ConnectBase, public EnableBase {
public:
  AudioInputSerialT() : AudioStream(0, nullptr) {}

  void init() override {
    //Serial.begin(115200);     // serialCmd *** assumed done ***
    SerialUSB1.begin(115200); // serialData
  }

  // DTR is a reliable indicator that Serial has connected to a host
  // *** it is not a reliable indicator of a disconnect ***
  // *** !Serial is not a reliable indicator of a disconnect ***

  // *** this is only good on first connection, then it's sticky ***
  bool linkStatus() override { return Serial.dtr(); }

  bool connect() override { return true; }

  //bool connected() override { return Serial && SerialUSB1; }
  bool connected() override { return true; }

  Stream* getStream() override { return &Serial; }
  ConnectMode getConnectionType() override { return CONNECT_USB; }

  void update() override {
    audio_block_t *blockL, *blockR;
    int n;

    if(!enabled) {
      char dump;
      // empty USB buffer
      while(available()) read(&dump, 1);
      return;
    }

    SETPROFILEPIN(PROFILER_ENTRY);

    if(available() < blockSize) {
      RESETPROFILEPIN(PROFILER_ENTRY);
      return;
    }

    blockL = allocate();
    blockR = allocate();
    if(!blockL || !blockR) {
      if(blockL) release(blockL);
      if(blockR) release(blockR);
      RESETPROFILEPIN(PROFILER_ENTRY);
      return;
    }

    n = read((void *)blockL->data, blockSize / 2);
    n += read((void *)blockR->data, blockSize / 2);
    if(n == blockSize) {
      TOGGLEPROFILEPIN(PROFILER_RX_TX);
      transmit(blockL, 0);
      transmit(blockR, 1);
    }

    release(blockL);
    release(blockR);

    RESETPROFILEPIN(PROFILER_ENTRY);
    RESETPROFILEPIN(PROFILER_RX_TX);
  }

private:
  static constexpr int blockSize = AUDIO_BLOCK_SAMPLES * sizeof(int16_t) * 2;
};

using AudioInputSerial = AudioInputSerialT< usb_serial_available, usb_serial_read >;

using AudioInputSerial1 = AudioInputSerialT< usb_serial2_available, usb_serial2_read >;

using AudioInputSerial2 = AudioInputSerialT< usb_serial3_available, usb_serial3_read >;

#endif
