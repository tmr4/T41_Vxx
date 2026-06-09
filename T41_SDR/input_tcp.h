#pragma once

/*
 AudioInputTCP - Streams 2-channels from set TCP port to connected AudioStream objects

Remote timing (w/ T41 standard testing input, Auto NF):
  * Once enabled and connected, data read from TCP port, 512-bytes total, directly to blocks allocated from Audio memory
    * { L-channel block, R-channel block } or { 256-bytes left channel, 256-bytes right channel }
    * Pointers to these blocks stored in circular buffer
  * read() must be called with sufficient frequency to avoid TCP buffer overflow
    * A similar frequency to update() maintains smooth data flow and minimizes Audio memory needs
  * update() run every 667us (2.9ms /44.1kHz * 192kHz)
    * queued input { 256-bytes left channel } and { 256-bytes right channel } sent to connected objects
  * ~15us to read 512-bytes from TCP port
  * ~1.4ms to process the 16 blocks of data required to form a frame for display
  * Time to complete one update of display (frame):
    * ~75ms or ~13.4 frames/sec
  * Notes:
    * Ethernet transport: on average two 512-byte packets arrive ~90us apart every 2ms
    * The T41 and remote prepare to render the next frame immediately after completing the
      previous frame. The different frame rates mean that the two units aren't rendering the same
      data slices at any given time.
    * The T41 and remote are running at different clock rates, 528MHz for the T41 for Teensy
      longevity and 600MHz on the remote due to AP instability at 528MHz (Teensy chip voltage issue)
    * The objects seemlessly handle disconnects in most cases. A long disconnect seems to require
      a longer time to reconnect.

 Works with AudioOutputTCP

 *******************************************************************************************
 The AudioInputTCP and AudioOutputTCP objects experience periodic interruptions. The first
 normally occurs ~30-45 minutes after startup. There isn't a pattern after that. The next
 may be up to several hour later.  The typical delay is ~1.7 seconds in duration.

 When the delay occurs, the remote unit normal program flow stops. The program does not
 return to the main loop or enter the AudioInputTCP::read method. The T41 unit slows down
 a bit, but continues entering the AudioOutputTCP::write method, but does not write anything.

 The system recovers automatically from these events.

 I haven't been able to track down the exact cause but can rule out the following:
  1) lost connection
  2) insufficient or too frequent Ethernet polling
     All gaps in Ethernet polling have been filled. Ethernet is polled in YieldToProcess,
     ProcessReceiverData and RA8875::waitBusy. One could argue that it is polled too often,
     but that would be wrong. Attempts to poll at fixed intervals fail, no matter how short
     the interval is (even as short as 10us). First, observe that Ethernet.loop is called
     by many QNEthernet methods and has itself a timing gate to prevent excessive calls.
     Second, the read/write available methods used here call Ethernet.loop and don't seem
     to suffer from excessive calls.
  3) the remote unit is not stuck in QNEthernetClient::read
  5) the T41 unit is not stuck in QNEthernetClient::writeFully
  4) Not caused by a full AudioInputTCP or AudioOutputTCP buffer, though the AudioOutputTCP
     buffer will fill and be dumped 42 times during a typical pause.
  5) AudioInputTCP not reading the Ethernet buffer. While this doesn't occur until the
     pause starts, I've recreated it by forcing a stall (not reading the buffer). While I can
     recreate an ~1.7 second pause by doing this, the nature of the statistics during the
     pauses are different.
  6) Insufficient audio memory. There is plenty of audio memory when the pauses occur. A
     switch to NON_STALLING from ORIGINAL and lowering the max buffers for Q_out_L didn't help.
  7) I noticed that the most recent pause started in the waterfall yield. The next didn't, but
     this was after I put the yield within the BTE_move register conditional. Still, with a pause
     occuring related to Ethernet.loop it doesn't appear to matter from where it's called in
     my code.

 The Telemetry class allows tracking statistics during the pause, but it hasn't proved useful
 in determining the root cause. AI speculates the cause is a cascading TCP ACK failure.

 What else has been tried w/o success (in addition to above):
  1) ARP table refresh every 10 minutes w/ etharp_request(netif_default, &xxxIP4Addr)
     A few tests with this refresh ran several hours though rather than failing in 30-45min.
     Another test run without this modification ran for over 4 hours without a pause. So the mod
     probably isn't a fix.
  2) Added a logic analyzer pin inside Ethernet.loop. Of course this has no effect on the
     pause, but it shows that the remote unit is stuck in Ethernet.loop during the pause.
     The T41 unit continues to operate normally, except it doesn't enter the write loop.
     (pin set/reset added at void EthernetClass::loop(), starting at line 182 in QNEthernet.cpp)
  3) Adding an extra call to Ethernet.loop when one of the calls to an "available" method
     returns less than 512 bytes. It's possible this changed the nature of the pauses seen
     in #1.

 Other things AI suggested to try:
  1) disable ? with client->setNoDelay(true), I already do this.
  2) Enable Statistics: src/lwipopts.h set LWIP_STATS and TCP_STATS to 1
  3) Reduce RTO: In src/qnethernet_opts.h, set #define LWIP_TCP_RTO_TIME 500 or 100.
  4) Increase Memory: Increase PBUF_POOL_SIZE to 128 or 256.
  5) #include <lwip/stats.h> stats_display(); // In loop() every 60 seconds
  6) #define TCP_SND_BUF (16 * 512) // Increase sender buffer space
     #define TCP_WND     (16 * 512) // Increase receiver window
  7) Disable OOSEQ Queue: If your application can handle the sender resending data, set #define TCP_QUEUE_OOSEQ 0. This tells the receiver: "If a packet is out of order, just drop it." This prevents the receiver from ever building that CPU-heavy linked list, ensuring Ethernet.loop() stays fast.
  8) Look for TCP retransmit (with #2):

#include "lwip/stats.h"

void checkTCPGlitches() {
    static uint32_t last_retrans = 0;
    // Access the internal lwIP TCP statistics
    uint32_t current_retrans = lwip_stats.tcp.xmit; // xmit tracks retransmissions in some versions

    // a pause without a retransmit means the TCP stack isn't the issue.
    if (current_retrans > last_retrans) {
        Serial.printf("TCP RETRANSMIT DETECTED: %d new events\n", current_retrans - last_retrans);
        last_retrans = current_retrans;
    }
}

  *** these all proved fruitless, likely because they needed to be combined with other options
      to be effective. This requires a much deeper understanding of the QNEthernet library ***

  *** Time to move on to UDP ***

 *******************************************************************************************
*/

#include <Arduino.h>
#include <AudioStream.h>

#include <QNEthernet.h>
using namespace qindesign::network;

#include "debug.h"
#include "telemetry.h"

//-------------------------------------------------------------------------------------------------------------
// Code
//-------------------------------------------------------------------------------------------------------------

class AudioInputTCP : public AudioStream {
public:
  AudioInputTCP() : AudioStream(0, nullptr) {}

  void begin() { enabled = true; }
	void end() {
    enabled = false;
    clear();
  }

  void setClient(EthernetClient* _client) {
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

  // read TCP data into queue
  // *** EthernetClient::available and read call Ethernet.loop() through
  //     getStateAndLoopOrClose so a call isn't needed here ***
  void read() {
    if(client) {
      if(enabled) {
        audio_block_t *blockL, *blockR;
        int available = client->available();
        int n;
        size_t t;
        bool bFull;
        unsigned long start;
        bool flag = available < blockSize;

        SETPROFILEPIN(PROFILER_ENTRY);

        // lock stuff for this loop
        noInterrupts();
        t = tail;
        bFull = bufferFull();
        interrupts();

        if(0) {
          // forced a TCP stall for telemetry testing
          static unsigned long lastStall = millis();
          unsigned long now = millis();
          static bool forceStall = false;
          // force a TCP stall every 60s
          if(now - lastStall > 60000) {
            forceStall = true;
            Serial.println("Forcing stall...");
            lastStall = now;
          }
          // ignore ethernet for 0.74s (causes a ~1.5-1.7s stall as system recovers)
          if(forceStall) {
            if(now - lastStall < 740) {
              return;
            } else {
              forceStall = false;
            }
          }
        }

        // *** buffer full check (usually happens on system glitch) ***
        // I've tested just dropping the oldest block and adding the new one.
        // The system recovers after the glitch, but spends sometime doing
        // the swap with no real gain. The damage (audio artifact) is already done.
        // Better is to just clear the entire buffer and allow the system to recover
        // faster instead of trying to force through old data. Setting a flag
        // to note buffer was fully during an update allows clear() to run from
        // an interrupt.
        if(bFull) {
          telemetry.bufferClearEvent(head, t, available);
          clear();
          t = 0;
        } else {
          telemetry.preLoopCheck(head, t, available);
        }

        while(available >= blockSize) {
          start = micros();

          telemetry.inLoopCheck(head, t, available);

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
          if(micros() - start > 50) Serial.println("long read in AudioInputTCP");

          if(n < blockSize) {
            // read error
            release(blockL);
            release(blockR);
            Serial.println("incomplete read in AudioInputTCP");
            break;
          }

          queue[head][0] = blockL;
          queue[head][1] = blockR;
          head = (head + 1) & bufferMask;
          available -= 512;
        }
        if(flag) Ethernet.loop();
      } else {
        uint8_t dump[512];
        // empty Ethernet buffer
        while(client->available() >= 512) {
          client->read(dump, 512);
        }
        // get the last bit
        while(client->available()) {
          client->read(dump, 1);
        }
      }
    }

    RESETPROFILEPIN(PROFILER_ENTRY);
    RESETPROFILEPIN(PROFILER_RX_TX);
  }

private:
	static constexpr size_t maxBlocks = 64; // *** must be power of 2 ***
	static constexpr size_t bufferMask = maxBlocks - 1;
  static_assert((maxBlocks & (maxBlocks - 1)) == 0, "maxBlocks must be a power of 2");

  bool enabled = false;

  EthernetClient *client = nullptr;

	audio_block_t* volatile queue[maxBlocks][2] = {};
	volatile size_t tail = 0;
	size_t head = 0;

  bool bufferClearEvent = false;

  static constexpr int blockSize = AUDIO_BLOCK_SAMPLES * sizeof(int16_t) * 2;

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
};
