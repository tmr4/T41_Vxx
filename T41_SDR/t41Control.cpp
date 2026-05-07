
#include <TimeLib.h>                   // Part of Teensy Time library

#include "SDT.h"

#include "ButtonProc.h"
#include "Display.h"
#include "Encoders.h"
#include "EEPROM.h"
#include "Filter.h"
#include "hardware.h"
#include "keyboard.h"
#include "MenuProc.h"
#include "mouse.h"
#include "Process.h"
#include "t41Control.h"
#include "Tune.h"
#include "Utility.h"

#if CAT_CONTROL_T41_USB_HOST
#include <USBHost_t36.h>
extern USBSerial_BigBuffer usbHostSerial;
extern USBSerial_BigBuffer usbHostSerial1;
#endif

#if SEND_IQ_TO_REMOTE || REC_IQ_FROM_T41
#define XXH_INLINE_ALL
#include "xxhash.h"
#endif

#include "debug.h"

//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

// for testing
//volatile bool sendGet = false;
bool sendGet = false;

// *** this is display dependent, but also fundamental to much of how the DSP process works ***
#define SPECTRUM_RES          512

bool useKenwoodIF = false;
bool controlDataFlag = false;

// calibration data
bool signalStrengthReceived = false;
float signalStrength = 0.0;
int signalStrengthReceivedIndex = -1;

bool checkingConnection = false;

/*

Remote IQ data stream transfer:
The T41 IQ data stream is transfered to a remote unit over USB Host. The remote
unit receives the data on USB serial. The specific USB objects are specified
in the hardware config file, hardwareConfig.h, for each unit. The transfer is
performed as follows:

T41:
  1. ProcessReceiverData pulls IQ data from Q_in_L and Q_in_R input queues.
  2. T41ControlBufferIQData buffers IQ data in iqBuffer.
  3. T41ControlSendIQData sends buffered IQ data to remote unit over USB Host.

Remote:
  1. T41RemoteReceiveIQData buffers IQ data from T41 over USB serial in iqBuffer.
  2. ProcessReceiverData calls T41ControlBlocksAvailable for available IQ data blocks.
  3. T41ControlBlocksAvailable aligns IQ data stream using start/end sync blocks.
  4. ProcessReceiverData calls T41ControlReadBufferL and T41ControlReadBufferR for pointers to IQ data blocks.

The transfer of IQ data into/from the buffer is very quick.  The transfer
over USB is much slower, but still well within the 10ms DSP loop processing time.
Usually, the IQ data is transfered from the T41 to the remote unit immediately after
ProcessReceiverData completes on the T41 and immediately before ProcessReceiverData starts
on the remote.  Occassionally, the USB transfer is a bottleneck and the next set of IQ
data will be produced on the T41 before all of the data from the previous loop has been
transfered.  While not critical for remote operation, an audio artifact will occur if
this data is lost. To avoid this, extra IQ data must be buffered.

To avoid unneeded memory copies, IQ data is copied into a circular buffer as it is produced,
block by block, first a block of I data, followed by a block of Q data.  Each DSP loop produces
8192 bytes of IQ data (16 blocks * 128 int16_t per block * 2 bytes / int16_t * 2 streams (I and Q)).

The transfer between the T41 and remote must be syncronized because of the interleaved IQ data.
The USB transfer is done in 512-byte chunks, the size used by Teensy 4.1.  This is also,
by coincidence, the size of one block of IQ data. The data stream is syncronized by adding
unique 512-byte sync blocks to the start and end of the IQ data, or 9216-bytes total.
This increases the data transfer by 12.5%, but keeps the data aligned to enable the use of
efficient pointers and indexing to process the IQ data.

The circular buffer size is set to a power of 2 to allow fast a fast indexing mask rather
than the typical modulo operation for wrapping at the end of the circular buffer. An extra
"sentinel" block is added to aid full/empty buffer logic.

The smallest power-of-2 buffer size to fit the 16 block IQ data, 2 block sync data and sentinel
block is 32. A 32-block buffer (16k) holds about 1.7 DSP loops of IQ data plus one sentinel
block. A 64-block buffer (32k) holds 3.5 DSP loops of IQ data plus one sentinel block. Testing
is needed to determine the best fit for various operating conditions.

Circular buffer placement in DMAMEM and EXTMEM has been tested with no material difference in
processing speed. Variablility in the USB transfer overwhelms any difference in these.
The buffer must be aligned for efficient pointer and syncing operations.

A hash of the start/end sync blocks is used to increase the speed of syncing the data stream
between the two units.  The start/end sync blocks are stored, but could be more generated
in place if memory is very tight.  Or they could be precomputed and place in flash.
*** TODO: test this *** Some memory efficiency can be gained by refining the common code
base for the two different hardware versions.

Efficient USB transfer between the two units requires that the data is pushed/pulled from
the USB pipeline consisently. This is done with calls to ProcessRemoteData from YieldToProcess
which is called periodically and by long running processes. This is important for proper
remote operation, though the T41 will continue to operated normally if a problem arises.
*** TODO: check that the CAT connection status is updated in this case ***

The remote renders the display at about 12 fps even with the T41 display on. The T41 frame rate
is about half of that, likely due to the USB Host overhead of scheduling the data packets.
The remote doesn't have that overhead and consumes the available data very rapidly. The
T41 requires at least a 64 block buffer, 32 blocks is insufficient and results in distortion
at the remote.

A very occasional glitch occurs in the data stream on the remote. The cause may be external.
Increasing the T41 buffer size to 128 blocks may help, but that's a big memory price for a small
glitch.
*** TODO: needs more investigation ***.

The circular buffer in DMAMEM is about a half a frame per second faster on the T41 than with
it in EXTMEM.

*** TODO: verify timing with new update ***

T41 timing (w/ T41 standard input and display disabled; Remote w/ Auto NF):
  * ~350us to buffer IQ data
  * ~2ms to process this data in ProcessReceiverData (extra time compared to remote is buffering)
  * ~3ms to transmit 16 blocks of IQ data to remote
  * loop time isn't meaningful as the T41 is continuously processing/transmitting IQ data every ~10ms

Remote timing (w/ T41 standard input and display disabled; Remote w/ Auto NF):
  * ~3ms to receive 16 blocks of IQ data from T41
  * ~1.5-2ms to process this data in ProcessReceiverData
  * ~85ms to complete one update of display (~12 frames/sec)

The flow of IQ data over USB doesn't begin until a remote connection between the units is
confirmed. This is polled periodically. *** TODO: refine this *** Data flow begins when
the connection is verified. While complete data sets are always buffered, the start up
data flow may exceed the circular buffer size, resulting in an incomplete data set at the
head of the circular buffer (the T41 T41ControlBufferIQData dumps to the circular buffer
regardless of a full buffer, though a warning is sent to Serial).  The remote accommodates
this by verifying a start sync block at the buffer tail and searching for one if not found.
This is fast with precomputed sync block hashes.

*** The remote T41RemoteReceiveIQData blocks USB receipt on full buffer. This is
inconsistent with above. ***
*** TODO: verify that this can be relaxed ***

During testing so far, a slip in the data stream hasn't been observed. That is, the start
sync block has stayed on the 512-byte boundary. A function to check for slippage is
available.

*/

#define BLOCK_SIZE    512
//#define BLOCKS      32
//#define BLOCK_MASK  31
//#define BLOCKS        64
//#define BLOCK_MASK    63
#define BLOCKS        128
#define BLOCK_MASK    127

// *** buffer and hash blocks need to be aligned ***
//EXTMEM uint8_t iqBuffer[BLOCKS][BLOCK_SIZE] __attribute__((aligned (32)));
DMAMEM uint8_t iqBuffer[BLOCKS][BLOCK_SIZE] __attribute__((aligned (32)));
//static size_t head, tail;
static size_t tail;
static volatile size_t head;

//static uint8_t start[BLOCK_SIZE] __attribute__((aligned (32)));
//static uint8_t end[BLOCK_SIZE] __attribute__((aligned (32)));
static DMAMEM uint8_t start[BLOCK_SIZE] __attribute__((aligned (32)));
static DMAMEM uint8_t end[BLOCK_SIZE] __attribute__((aligned (32)));
uint64_t startFirst, endFirst, startHash, endHash, startQuickHash, endQuickHash;

//bool remoteReady = false;
bool remoteReady = true;

IntervalTimer remoteTimer;

//-------------------------------------------------------------------------------------------------------------
// Forwards
//-------------------------------------------------------------------------------------------------------------

void SendID(bool request);
void UsbHostTask();
void GenerateStartEndSyncBlock(uint8_t* buf, uint64_t salt);
uint64_t IQQuickHash(uint8_t *buf);

void ReceiveRemoteIQDataISR();

//-------------------------------------------------------------------------------------------------------------
// Code
//-------------------------------------------------------------------------------------------------------------

// the three usb serial objects in the teensy (Serial, SerialUSB1 and SerialUSB2) are all different classes (usb_serial_class, usb_serial2_class, and usb_serial3_class)
// I suppose to prevent naming conflict somewhere, but this prevents having serial commands with a common argument specifying the serial channel to use, such as
// void T41ControlSetup(Stream& serial) { serial.begin(); }.  As such might as well duplicate these functions for both the T41 control app and Beacon monitor
void T41ControlSetup() {
  //controlSerial.begin(19200);
  if(CAT_CONTROL_REMOTE_USB) {
    sendGet = false;
  } else {
    sendGet = true;
    //sendGet = false;
  }
#if SEND_IQ_TO_REMOTE || REC_IQ_FROM_T41
  const uint64_t seed = 0x9E3779B97F4A7C15ULL; // fractional part of the Golden Ratio (2^64 / phi)

  // set up start/end sync blocks
  GenerateStartEndSyncBlock(start, 0x1234567890123456ULL);
  startHash = XXH3_64bits_withSeed(start, BLOCK_SIZE, seed);
  startQuickHash = IQQuickHash(start);
  startFirst = *(uint64_t*)(start);

  GenerateStartEndSyncBlock(end, 0x9876543210987654ULL);
  endHash = XXH3_64bits_withSeed(end, BLOCK_SIZE, seed);
  endQuickHash = IQQuickHash(end);
  endFirst = *(uint64_t*)(end);

/*
  Serial.println();
  Serial.println();
  Serial.println("XXH3 has on T41:");
  Serial.print("startHash:      0x");
  Serial.print((uint32_t)(startHash >> 32), HEX);
  Serial.println((uint32_t)startHash, HEX);
  Serial.print("startQuickHash: 0x");
  Serial.print((uint32_t)(startQuickHash >> 32), HEX);
  Serial.println((uint32_t)startQuickHash, HEX);
  Serial.println();

  Serial.print("endHash:        0x");
  Serial.print((uint32_t)(endHash >> 32), HEX);
  Serial.println((uint32_t)endHash, HEX);
  Serial.print("endQuickHash:   0x");
  Serial.print((uint32_t)(endQuickHash >> 32), HEX);
  Serial.println((uint32_t)endQuickHash, HEX);
  Serial.println();
  Serial.println();
*/
#endif
#if REC_IQ_FROM_T41
  remoteTimer.begin(ReceiveRemoteIQDataISR, 50);
  //remoteTimer.begin(ReceiveRemoteIQDataISR, 75);
  //remoteTimer.begin(ReceiveRemoteIQDataISR, 100);
  //remoteTimer.begin(ReceiveRemoteIQDataISR, 125);
#endif
}

void T41RemoteConnectCheck() {
  static unsigned long last = 0;
  unsigned long now = millis();
  int lasped = now - last;

  if(t41.RemoteStatus != REMOTE_CONNECTED) {
    // send a connection request every 5s until connected
    if(lasped > 5000) {
      t41.RemoteStatus = REMOTE_WAITING;
      SendID(true);
      last = now;
    }
  } else {
    // check for lost connection
    if(checkingConnection) {
      // check if response received within 5s
      if(lasped > 5000) {
        // connection lost
        t41.RemoteStatus = REMOTE_LOST;
        remoteReady = false;
        checkingConnection = false;
        last = now;
      }
    } else {
      // check connection every 30s
      if(lasped > 30000) {
        checkingConnection = true;
        remoteReady = true;
        SendID(true);
        last = now;
      }
    }
  }
}

void T41ControlSendData(uint8_t *data, int len) {
  //int len = strlen(cmd);
  //int sizeBuf = SerialUSB1.availableForWrite();
  //Serial.print("Sending spectrum data, length: "); Serial.print(len); Serial.print(", buffer size: "); Serial.println(sizeBuf);
  //for(int i = 0; i < SPECTRUM_RES; i++) {
  //  Serial.write(data[i]); Serial.print(" "); Serial.println(data[i]);
  //}
  // *** TODO: work up alternative if USB buffer is sufficient ***
  if(controlSerial.availableForWrite() > len) {
    controlSerial.write(data, len);
    //SerialUSB1.write(data, SPECTRUM_RES);
    //SerialUSB1.send_now(); // we'll have a delay without this *** TODO: try with and without ***
  }
  //controlDataFlag = false;
}

// data[SPECTRUM_RES]
void T41PrepareSpectrumData(int16_t *data, int16_t max) {
  uint8_t specData[518]; // xDyyy[up to 512 bytes of data];   x=A or F, yyy = 255 - max
  int tmp = 0;

  // FDxxx[512]; where xxx = 255 - max and [512] = 512 bytes spectrum data
  sprintf((char*)specData, "FD%03d", 255 - max);
  specData[517] = ';';

  // shift spectrum data and send it to PC
  // we have to scale and apply noise floor in the control app
  for(int i = 0; i < SPECTRUM_RES; i++) {
    // shift data so max = 255
    // *** TODO: consider scaling here fits data into a 0-255 range ***
    tmp = data[i] + 255 - max;
    // though unlikely, data can still be negative, limit it
    if(tmp < 0) {
      tmp = 0;
    }
    //if(tmp > 255) {
    //  tmp = 255;
    //}
    specData[i + 5] = (uint8_t)tmp;
  }

  T41ControlSendData(specData, SPECTRUM_RES + 6);
}

void T41ControlSendCmd(char *cmd) {
  SETPROFILEPIN(PROFILER_FT8_CAT_TX);
  if(sendGet) {
    Serial.print("Sending: ");
    Serial.println(cmd);
  }
  int sizeBuf = controlSerial.availableForWrite();
  if(cmd[0] != 0 && sizeBuf > 0) {
    // the size of Teensy 4.1 serial transmit buffer is 8k and is used in 4 2k parts.
    // I've seen about 6k available at this point.
    // (https://forum.pjrc.com/index.php?threads/usb-serial-on-teensy-4-0-buffer-size-limitation.67826/)
    int len = strlen(cmd);
    //Serial.println(sizeBuf);
    if(controlSerial.availableForWrite() > len) {
      //if(sendGet) {
      //  Serial.println(len);
      //}
      controlSerial.write(cmd, len);
#if controlSerial != usbHostSerial
      controlSerial.send_now(); // we'll have a delay without this
#endif
    } else {
      int i=0;
      //Serial.println(sizeBuf);
      while(cmd[i] != 0) {
        if(controlSerial.availableForWrite() > 0) {
          //SerialUSB1.print(cmd[i++]);
        } else {
          controlSerial.flush(); // *** TODO: this will cause a freeze if PC stops receiving ***
          if(sendGet) {
            Serial.println("flushing");
          }
          controlDataFlag = false;
        }
      }
    }
  } else {
    controlDataFlag = false;
  }
  RESETPROFILEPIN(PROFILER_FT8_CAT_TX);
}

int T41ControlGetCommand(char * cmd, int max) {
  int i = 0;

  while(controlSerial.available()) {
    cmd[i] = (char) controlSerial.read();

    // there might be multiple commands in the serial buffer
    // read only the first one or up to the specified limit
    if(cmd[i] == ';' || i >= max) {
      break;
    }
    i++;
  }
  cmd[i+1] = 0; // *** TODO: this is currently needed by send command, revisit if that is changed ***
  return i;
}

// Dual T41 master commands
// for sending integer-based commands between T41 and remote
void SendCommand(int value, int id) {
  char cmd[30]; // 50 if we include IF

  switch(id) {
    case T41_ITEM_VOL:
      sprintf(cmd, "VO%03d;", value);
      break;
    case T41_ITEM_AGC:
      sprintf(cmd, "GT%d;", value);
      break;
    case T41_ITEM_TUNE:
      sprintf(cmd, "FI0%1d;", value);
      break;
    case T41_ITEM_FINE:
      sprintf(cmd, "FI1%1d;", value);
      break;
    case T41_ITEM_ZOOM:
      sprintf(cmd, "ZM%d;", value);
      break;
    case T41_ITEM_FLOOR:
      sprintf(cmd, "NG%d;", value);
      break;
    case T41_ITEM_NOTCH:
      cmd[0] = 0;
      break;
    case T41_ITEM_FILTER:
      sprintf(cmd, "N1%d;", value);
      break;
    case T41_ITEM_COMPRESS:
      cmd[0] = 0;
      break;
    case T41_ITEM_RFGAIN:
      sprintf(cmd, "PG%+03d;", value);
      break;
    case T41_ITEM_EQUALIZER:
      cmd[0] = 0;
      break;
    case T41_ITEM_DECODER:
      cmd[0] = 0;
      break;
    case T41_ITEM_KEY:
      cmd[0] = 0;
      break;
    case T41_ITEM_KEYER:
      cmd[0] = 0;
      break;
/*
    case T41_ITEM_FT8:
      cmd[0] = 0;
      break;
    case T41_ITEM_FT8_INT:
      cmd[0] = 0;
      break;
    case T41_ITEM_FT8_TX:
      cmd[0] = 0;
      break;
    case T41_ITEM_FT8_CQ:
      cmd[0] = 0;
      break;
    case T41_ITEM_FT8_TXF:
      cmd[0] = 0;
      break;
    case T41_ITEM_FT8_RXF:
      cmd[0] = 0;
      break;
    case T41_ITEM_STACK:
      cmd[0] = 0;
      break;
    case T41_ITEM_HEAP:
      cmd[0] = 0;
      break;
    case T41_ITEM_TEMP:
      cmd[0] = 0;
      break;
    case T41_ITEM_LOAD:
      cmd[0] = 0;
      break;
*/
    case T41_ITEM_MOUSE:
      // *** FS is fine tune selected ***
      sprintf(cmd, "FS%d;", !value);
      break;
    case T41_ITEM_NOISE:
      sprintf(cmd, "NF%04d;", value);
      break;
    case T41_ITEM_RADIO_MODE:
      sprintf(cmd, "ME%d;", value);
      break;
    case T41_ITEM_DEMOD_MODE:
      sprintf(cmd, "MD%d;", value);
      break;
    case T41_ITEM_BAND:
      sprintf(cmd, "BD%d;", value);
      break;
    case T41_ITEM_POWER:
      sprintf(cmd, "PC%02d;", value);
      break;
    case T41_ITEM_FREQ:
      sprintf(cmd, "FC%011d;", value);
      break;
    case T41_ITEM_NCO:
      sprintf(cmd, "FF%011d;", value);
      break;
    case T41_ITEM_FHI:
      sprintf(cmd, "NH%011d;", value);
      break;
    case T41_ITEM_FLO:
      sprintf(cmd, "NL%011d;", value);
      break;
    case T41_ITEM_SCALE:
      cmd[0] = 0;
      break;
    case T41_ITEM_CW_FILTER:
      cmd[0] = 0;
      break;
/*
    case :
      cmd[0] = 0;
      break;
    case :
      cmd[0] = 0;
      break;
    case :
      cmd[0] = 0;
      break;
    case :
      break;
    case :
      break;
*/
    default:
      return;
  }
  T41ControlSendCmd(cmd);
}

void SendID(bool request) {
  char cmd[7];

  if(request) {
    sprintf(cmd, "ID;");
  } else {
    sprintf(cmd, "IDxxx;");
  }

  T41ControlSendCmd(cmd);
}

void SendFreqA(int freq) {
  char cmd[20];

  sprintf(cmd, "FA%011d;", freq);
  T41ControlSendCmd(cmd);
}

void SendFreqB(int freq) {
  char cmd[20];

  sprintf(cmd, "FB%011d;", freq);
  T41ControlSendCmd(cmd);
}

void SendBandChange(int upDown) {
  char cmd[5];

  if(upDown > 0) {
    sprintf(cmd, "BU;");
  } else {
    sprintf(cmd, "BD;");
  }

  T41ControlSendCmd(cmd);
}

//void SendSmeter(int16_t smeterPad, float32_t dbm) {
void SendSmeter(int smeterPad, float dbm) {
  char cmd[30];
  // we can send these separately or together
   //T41ControlSendCmd(cmd);

  // send dBm and s-meter together
  // it's more efficient to send these together, though it's more work on the
  // receiving end.  We have to do that work anyway as the second message
  // more often than not arrives in the PC buffer prior to the first message
  // being read from the buffer.  Thus the two messages are in essence
  // combined.
  sprintf(cmd, "SM0%+05d;SM20%04d;", (int)(dbm * 10), smeterPad);
  T41ControlSendCmd(cmd);
}

void SendFilter() {
  char cmd[6];

  sprintf(cmd, "NS%+1d;", posFilterEncoder - lastFilterEncoder);
  T41ControlSendCmd(cmd);
}

void SendSignalStrengthRequest() {
  char cmd[5];

  sprintf(cmd, "SM;");
  T41ControlSendCmd(cmd);
}

void SendSignalStrengthRequest(int index) {
  char cmd[5];

  sprintf(cmd, "SM%d;", index);
  T41ControlSendCmd(cmd);
}

// sets 0.5kHz-1.5kHz audio filter
void SendNarrowFilter() {
  char cmd[4];

  sprintf(cmd, "NW;");
  T41ControlSendCmd(cmd);
}

void SendAS() {
  char cmd[19];

  sprintf(cmd, "AS%011d%d%d%d;",
    t41.ActiveFreq(), // freq in Hz (%011d) at index 2
    (int)t41.ActiveBand,                    // current band (%d) at index 13
    (int)t41.RadioMode,                        // transmission mode (%d) at index 14
    (int)t41.DemodMode         // demodulation mode (%d)  at index 15
  );
  T41ControlSendCmd(cmd);
}

void SendIF() {
  char cmd[50];

  // *** Warning: this is not the Kenwood implimentation ***
  sprintf(cmd, "IF%011d%d%d%d%03d%+06d%04d%d%d%d%d%d%d%d%d%011d;",
    // active VFO Freq = TxRxFreq, t41.CenterFreq = TxRxFreq - NCOFreq
    //  *** TODO: we only need 8 digits for first field for T41, consider using other 3 for something ***
    t41.ActiveFreq(), // freq in Hz (%011d) at index 2
    (int)t41.ActiveBand,            // current band (%d) at index 13
    (int)t41.RadioMode,             // transmission mode (%d) at index 14
    (int)t41.DemodMode,             // demodulation mode (%d)  at index 15
    (int)t41.AudioVolume,           // audio volume (%03d) at index 16
    (int)t41.NCOFreq,               // NCO freq (%+06d) at index 19
    (int)t41.NoiseFloor,            // noise floor (%04d) at index 25 *** TODO: verify need for +- or number of digits ***
    (int)t41.LiveNoiseFloor,        // set noise floor active/inactive 1/0 (%d) at index 29
    !GetXRState(),                  // RX/TX (1/0) (%d) at index 30
    (int)t41.ActiveVFO,             // VFO A/B (0/1) (%d) at index 31
    (int)t41.MouseCenterTuneActive, // fine or center tune enabled (0/1) (%d) at index 32
    (int)t41.FineTuneIndex,         // fine tune index (%d) at index 33
    (int)t41.CenterTuneIndex,       // center tune index (%d) at index 34
    (int)t41.AGCMode,               // AGC mode (%d) at index 35
    (int)t41.SpectrumZoom,          // spectrum zoom (%d) at index 36
    (int)t41.InactiveFreq           // inactive VFO freq in Hz (%011d) at index 37
    //splitVFO ? 1 : 0,             // VFO split status (%d) at index xx
  );
  T41ControlSendCmd(cmd);
}

// Kenwood modes
int GetMode() {
  // 1: LSB, 2: USB, 3: CW, 4: FM, 5: AM
  int mode;
  if(t41.RadioMode == CW_MODE) {
    mode=3;
  } else {
    switch(t41.DemodMode) {
      case DEMOD_USB:
        mode=2; // USB
        break;
      case DEMOD_LSB:
        mode=1; // LSB
        break;
      case DEMOD_AM:
      case DEMOD_SAM:
        mode=5; // AM
        break;
      case DEMOD_NFM:
        mode=4; // FM
        break;
      default:
        mode=1; // LSB
        break;
    }
  }
  return mode;
}

// *** generally it's best to use the Update method to change T41 properties here.
//     Properties should not be updated directly or call a function that does so
//     especially if they notify the remote as this creates a update loop.
//     Many update functions provide a flag to supress remote notifications.
//     If the two units get out of sync the loop will become infinite as the units go
//     back and forth trying to impose their own value. Ignoring this can degrade
//     radio performance ***
// *** TODO: verify only single message goes back and forth for property updates ***
/*

  Property event times:
    *** a small time base is needed to view short duration events in the profile viewer ***
    FilterHiCut NH%011d; 2.5ms
    NCOFreq     FF%011d; 500ns

*/
void T41ControlLoop() {
  float32_t dbm;

  T41RemoteConnectCheck();

  if(controlSerial.available()) {
    char cmd[256];
    int mode = GetMode();

    T41ControlGetCommand(cmd, 256);

    SETPROFILEPIN(PROFILER_FT8_REMOTE_RX);

    if(sendGet) {
      Serial.print("Received: ");
      Serial.println(cmd);
    }

    switch(cmd[0]) {
      case 'B':
        if(cmd[1] == 'U' && cmd[2] == ';') {
          // band up
          ChangeBand(1);
          SendAS();
        } else if(cmd[1] == 'D' && cmd[2] == ';') {
          // band up
          ChangeBand(-1);
          SendAS();
        } else if(cmd[1] == 'D' && cmd[3] == ';') {
          ChangeBand(t41.ActiveBand - atoi(&cmd[2]), false);
        }
        break;

      case 'D':
        if(cmd[1] == 'S' && cmd[2] == ';') {
          // start sending spectrum data
          controlDataFlag = true;
        } else if(cmd[1] == 'P' && cmd[2] == ';') {
          // stop sending spectrum data
          controlDataFlag = false;
        }
        break;

      case 'F':
        long f;
        switch(cmd[1]) {
          case 'A':
            if(cmd[13] == ';') {
              // set VFO A frequency
              f = atol(&cmd[2]);
              ChangeBand(f);
              if(t41.MouseCenterTuneActive) {
                t41.SetFreqA(f);
              } else {
                t41.NCOFreq.Update(f); // *** verify ***
              }
              //Serial.print("Set VFO A to "); Serial.println(f);
            } else if(cmd[2] == ';') {
              // read VFO A frequency
              sprintf(cmd, "FA%011d;", t41.GetFreqA());
              T41ControlSendCmd(cmd);
            }
            break;

          case 'B':
            if(cmd[13] == ';') {
              // set VFO B frequency
              f = atol(&cmd[2]);
              ChangeBand(f);
              if(t41.MouseCenterTuneActive) {
                t41.SetFreqB(f);
              } else {
                t41.NCOFreq.Update(f); // *** verify ***
              }
              // Serial.print("Set VFO B to "); Serial.println(f);
            } else if(cmd[2] == ';') {
              // read VFO B frequency
              sprintf(cmd, "FA%011d;", t41.GetFreqB());
              T41ControlSendCmd(cmd);
            }
            break;

          case 'C':
            if(cmd[13] == ';') {
              // set center frequency
              f = atol(&cmd[2]);
              t41.CenterFreq.Update(f);
              SetFreq(f);
              //Serial.print("Center freq set to "); Serial.println(f);
            } else if(cmd[2] == ';') {
              // read center frequency
              sprintf(cmd,"FC%011d;", (int)t41.CenterFreq);
              T41ControlSendCmd(cmd);
            }
            break;

          case 'F':
            if(cmd[13] == ';') {
              // set NCO frequency offset
              t41.NCOFreq.Update(atol(&cmd[2]));
            } else if(cmd[2] == ';') {
              // read NCO frequency offset
              sprintf(cmd, "FF%011d;", (int)t41.NCOFreq);
              T41ControlSendCmd(cmd);
            }
            break;

          case 'I':
            if(cmd[4] == ';') {
              // center or fine tune increment change
              if(cmd[2] == '0') {
                ChangeFreqIncrement(atol(&cmd[3]) - t41.CenterTuneIndex, false);
              } else if(cmd[2] == '1') {
                ChangeFtIncrement(atol(&cmd[3]) - t41.FineTuneIndex, false);
              }
            }
            break;

          case 'S':
            if(cmd[3] == ';') {
              // fine tune on or off
              t41.MouseCenterTuneActive.Update(!atoi(&cmd[2]));
              HighlightTuneInc();
            }
            break;

          case 'T':
            if(cmd[3] == ';') {
              // select VFO
              VFOSelect(atoi(&cmd[2]));
              SendAS();
            }
            break;

          default:
            //cmd[0] = '?';
            //cmd[1] = ';';
            //cmd[2] = 0;
            break;
        }
        break;

      case 'G':
        if(cmd[1] == 'T' && cmd[3] == ';') {
          // update AGC
          t41.AGCMode.Update(atoi(&cmd[2]));
          UpdateInfoBoxItem(T41_ITEM_AGC);
        }
        break;

      case 'I':
        if(cmd[1] == 'D' && cmd[2] == ';') { // ID;
          // reply with the TS-890S id
          sprintf(cmd,"ID024;");
          //sprintf(cmd,"ID019;"); // TS-2000
          T41ControlSendCmd(cmd);
          t41.RemoteStatus = REMOTE_CONNECTED;
        } else if(cmd[1] == 'D' && cmd[5] == ';') { // IDxxx;
          if(checkingConnection) {
            checkingConnection = false;
          }
          t41.RemoteStatus = REMOTE_CONNECTED;
        } else if(cmd[1] == 'F' && cmd[2] == ';') {
          // retrieves transceiver status
          if(useKenwoodIF) {
            // *** TODO: not set up, just for testing ***
            sprintf(cmd, "IF%011d%04d%+06d%d%d%d%02d%d%d%d%d%d%d%02d%d;",
              t41.ActiveFreq(),     // freq in Hz
              0,            // freq step size
              0,            // RIT/XIT freq in Hz, +-99999, this isn't preserved in the T41 but would be VFO A - VFO B if split
              0,            // RIT on/off
              0,            // XIT on/off
              0,0,          // channel bank number
              !GetXRState(),     // RX/TX (1/0)
              mode,         // operating mode
              (int)t41.ActiveVFO,    // RX VFO
              0,            // scan Status
              0,            // split status (Kenwood manual refers to SP command which doesn't exist)
              0,            // CTCSS enabled
              0,            // CTCSS tone frequency
              0             // shift status
            );
            T41ControlSendCmd(cmd);
          } else {
            SendIF();
          }
        }
        break;

      case 'M':
        if(cmd[1] == 'D' && cmd[2] == ';') {
          // send demod mode
          sprintf(cmd,"MD%d;", useKenwoodIF ? mode : t41.DemodMode);
          T41ControlSendCmd(cmd);
        } else if(cmd[1] == 'D' && cmd[3] == ';') {
          // set demod mode status
          ChangeDemodMode(atoi(&cmd[2]), false);
          //SendAS();
        } else if(cmd[1] == 'E' && cmd[3] == ';') {
          // set operating mode
          ChangeMode(atoi(&cmd[2]), -1, false);
          //SendAS();
        }
        break;

      case 'N':
        if(cmd[1] == 'F' && cmd[2] == ';') {
          // send noise floor
          sprintf(cmd,"NF%04d;", (int)t41.NoiseFloor);
          T41ControlSendCmd(cmd);
        } else if(cmd[1] == 'F' && cmd[6] == ';') {
          // set noise floor
          t41.NoiseFloor.Update(atoi(&cmd[2]));
        } else if(cmd[1] == 'G' && cmd[3] == ';') {
          t41.LiveNoiseFloor.Update(atoi(&cmd[2]));
          UpdateInfoBoxItem(T41_ITEM_FLOOR);
        } else if(cmd[1] == 'H' && cmd[13] == ';') {
          t41.FilterHiCut.Update(atol(&cmd[2]));

          CalcAudioFilters();
        } else if(cmd[1] == 'L' && cmd[13] == ';') {
          t41.FilterLoCut.Update(atol(&cmd[2]));

          CalcAudioFilters();
        } else if(cmd[1] == 'S' && cmd[4] == ';') {
          // inc/dec audio filter
          posFilterEncoder += atoi(&cmd[2]);
          ProcessFilterEncoder();

          CalcAudioFilters();
          UpdateDisplayFilters();
        } else if(cmd[1] == 'W' && cmd[2] == ';') {
          // sets 0.5kHz-1.5kHz audio filter
          t41.FilterLoCut.Update(500);
          t41.FilterHiCut.Update(1500);

          CalcAudioFilters();
        } else if(cmd[1] == '1' && cmd[3] == ';') {
          t41.NoiseFilter.Update(atoi(&cmd[2]));
          UpdateInfoBoxItem(T41_ITEM_FILTER);
        }
        break;

      case 'P': // PCxxx;
        if(cmd[1] == 'C' && cmd[4] == ';') {
          // set transmitter power level
          t41.TxPower.Update(atoi(&cmd[2]));
          ShowCurrentPowerSetting();
        }
        break;

      case 'S': // SM; or SMxyyyyy;
        dbm = CalcSignalStrength();

        if(cmd[1] == 'M' && cmd[2] == ';') {
          // One of the following:
          // send dBm
          //sprintf(cmd, "SM0%+05d;", (int)(dbm * 10));

          // send s-meter
          //sprintf(cmd, "SM20%04d;", smeterPad);

          // just send dBm for now
          sprintf(cmd, "SM0%+05d;", (int)(dbm * 10));
        } else if(cmd[1] == 'M' && cmd[3] == ';') {
          int index = atoi(&cmd[2]);

          // just send dBm for now
          sprintf(cmd, "SM%d%+05d;", index, (int)(dbm * 10));
        } else if(cmd[1] == 'M' && cmd[8] == ';') {
          // One of the following:
          // SM0-xxxx; (receive dBm)
          //sprintf(cmd, "SM0%+05d;", (int)(dbm * 10));

          // send s-meter
          //sprintf(cmd, "SM20%04d;", smeterPad);

          //Serial.print("Received signal strength: ");
          signalStrengthReceivedIndex = atoi(&cmd[2]);
          signalStrength = ((float)atoi(&cmd[3])) / 10.0;
          signalStrengthReceived = true;
          //Serial.println(signalStrength);
        }
        break;

      case 'T':
        if(cmd[1] == 'M' && cmd[13] == ';') {
          // set Teensy RTC
          //Serial.print("TM cmd from controlSerial: "); Serial.println(atol(&cmd[2]));
          //Serial.println(Teensy3Clock.get());
          Teensy3Clock.set(atol(&cmd[2]));
          setTime(atol(&cmd[2]));
        }
        break;

      case 'V': // VOxxx;
        if(cmd[1] == 'O' && cmd[5] == ';') {
          // set volume (without notify chain)
          t41.AudioVolume.Update(atoi(&cmd[2]));
        }
        break;

      case 'Z': // ZMx;
        if(cmd[1] == 'M' && cmd[3] == ';') {
          // set spectrum zoom
          t41.SpectrumZoom.Update(atoi(&cmd[2]));
        }
        break;

      case '?': // unknow command
        // do nothing for now
        break;

      default:
        // what was received in not handled or recognized
        // ... otherwise send back a question
        //cmd[0] = '?';
        //cmd[1] = ';';
        //cmd[2] = 0;
        break;
    }

    RESETPROFILEPIN(PROFILER_FT8_REMOTE_RX);
  }
}

// Remote data
bool noAccess = false;

// room to add another block
bool BufFull() {
  noAccess = true;
  bool result = ((head + 1) & BLOCK_MASK) == tail;
  noAccess = false;
  return result;
}

// data to read
bool BufEmpty() {
  noAccess = true;
  bool result = head == tail;
  noAccess = false;
  return result;
}

void SendMsg(const char *msg, int value) {
  char cmd[256];
  sprintf(cmd, msg, value);
  T41ControlSendCmd(cmd);
}

void CheckSlip(uint8_t *blk) {
  char cmd[256];
  //sprintf(cmd, "%d: 0x%lu%lu, 0x%lu%lu;", count, (uint32_t)(IQQuickHash(blk) >> 32), (uint32_t)IQQuickHash(blk), (uint32_t)(IQQuickHash(&blk[256]) >> 32), (uint32_t)IQQuickHash(&blk[256]));
  //T41ControlSendCmd(cmd);

  TOGGLEPROFILEPIN(PROFILER_PROCESS_FRAME);
  for(int i = 0; i < 512 - 8; i++) {
    uint64_t val = *(uint64_t*)(blk + i);

    if(val == startFirst) {
      sprintf(cmd, "Found startFirst at: %d;", i);
      T41ControlSendCmd(cmd);
    }
    if(val == endFirst) {
      sprintf(cmd, "Found endFirst at: %d;", i);
      T41ControlSendCmd(cmd);
    }
  }
}

void ReceiveRemoteIQDataISR() {
  static int count = 0;
  //if(noAccess) return;
  if(t41.RemoteStatus == REMOTE_CONNECTED) {
    if(BufFull()) {
      if(count == 0) {
        // zero everything first time buffer is full
        ++count;
        tail=head=0;
      }
      return;
    }
    //if(BufFull()) { return; }
    //SETPROFILEPIN(PROFILER_FT8_REMOTE_RX);
    int avail = controlAudio.available();
    if(avail >= 512) {
      SETPROFILEPIN(PROFILER_FT8_REMOTE_RX);
      //while(avail >= 512 && ++count < 10)
      {
        TOGGLEPROFILEPIN(PROFILER_FT8_REMOTE_RX);
        controlAudio.readBytes((char*)&iqBuffer[head], 512);
        head = (head + 1) & BLOCK_MASK;
        avail -= 512;
      }
    }
    RESETPROFILEPIN(PROFILER_FT8_REMOTE_RX);
  }
}

void T41RemoteReceiveIQData() {
/*
  int avail = controlAudio.available();
  //while(!BufFull()) {
  //while(controlAudio.available() >= 512) {
  while(avail >= 512) {
    SETPROFILEPIN(PROFILER_FT8_REMOTE_RX);
    if(controlAudio.readBytes((char*)&iqBuffer[head], 512) == 0) return;
    head = (head + 1) & BLOCK_MASK;
    avail -= 512;
  }

  RESETPROFILEPIN(PROFILER_FT8_REMOTE_RX);
*/
}

void CheckBlocksAvailable() {
  if(t41.RemoteStatus == REMOTE_CONNECTED) {
    if(controlAudio.available() >= 512) {
      T41RemoteReceiveIQData();
    }
  }
}

bool FindStart() {
  bool result = false;
  SETPROFILEPIN(PROFILER_DECODE_FT8);
  while(!BufEmpty()) {
    TOGGLEPROFILEPIN(PROFILER_DECODE_FT8);
    if(IQQuickHash(iqBuffer[tail]) == startQuickHash) {
      result = true;
      break;
    }
    tail = (tail + 1) & BLOCK_MASK;
  }
  RESETPROFILEPIN(PROFILER_DECODE_FT8);
  return result;
}

//TOGGLEPROFILEPIN(PROFILER_DECODE_FT8);
//TOGGLEPROFILEPIN(PROFILER_PROCESS_FRAME);

int T41ControlBlocksAvailable() {
  int blocks = 0;

  if(t41.RemoteStatus == REMOTE_CONNECTED) {
    while(!BufEmpty()) {
      noAccess = true;
      if(((head - tail) & BLOCK_MASK) >= 18) { // 16 data + start/end blocks
        noAccess = false;
        if(IQQuickHash(iqBuffer[tail]) == startQuickHash) {
          // sync start verified, consume it
          tail = (tail + 1) & BLOCK_MASK;

          // look at end
          if(IQQuickHash(iqBuffer[tail+16]) == endQuickHash) {
            // sync end verified
            blocks = 16;
            break;
          } else {
            // search for start
            if(!FindStart()) break;
          }
        } else {
          // current block isn't start, consume it
          tail = (tail + 1) & BLOCK_MASK;
          // search for start
          if(!FindStart()) break;
        }
      } else {
        break;
      }
    }
  }

  noAccess = false;
  return blocks;
}

int16_t *T41ControlReadBufferL(int block) {
  int16_t *tmp = (int16_t *)iqBuffer[tail];

  TOGGLEPROFILEPIN(PROFILER_PROCESS_FRAME);

  // *** error checking??? ***

  return tmp;
}

int16_t *T41ControlReadBufferR(int block) {
  int16_t *tmp = (int16_t *)&iqBuffer[tail][256];

  TOGGLEPROFILEPIN(PROFILER_PROCESS_FRAME);

  tail = (tail + 1) & BLOCK_MASK; // consume L/R block

  if(block == 15) {
    tail = (tail + 1) & BLOCK_MASK; // consume sync block
  }

  return tmp;
}

//void T41ControlFreeBufferL() {
//}
//void T41ControlFreeBufferR() {
//}

void T41ControlSendIQData() {
  int avail;
  //static long prevUpdate = 0;
  long maxTime = micros();
  int count = 0;
  static int count2 = 0;

  SETPROFILEPIN(PROFILER_FT8_CAT_TX);
  UsbHostTask();
  if(++count2 < 10) Serial.println("starting transfer...");
  while(true) {
    if(BufEmpty() || count > 18) {
      if(count2 < 10) Serial.println("...transfer done");
      break;
    }
    //if(t41.RemoteStatus != REMOTE_CONNECTED) break;
    //while(controlAudio.availableForWrite() < 512) {
    //  //TOGGLEPROFILEPIN(PROFILER_PROCESS_FRAME);
    //  TOGGLEPROFILEPIN(PROFILER_FT8_CAT_TX);
    //  UsbHostTask();
    //  T41ControlLoop();
    //  if(t41.RemoteStatus != REMOTE_CONNECTED) break;
    //  //delayMicroseconds(50);
    //  if(micros() - maxTime > 300) break;
    //}
    avail = controlAudio.availableForWrite();
    if(avail >= 512) {
      if(count2 < 10) Serial.printf("Avail to write (%d): %d\n", count, avail);
      controlAudio.write((char*)&iqBuffer[tail], 512);
      tail = (tail + 1) & BLOCK_MASK;
      //avail -= 512;
      //controlAudio.write((char*)&iqBuffer[tail], (head-tail)*512);
      //Serial.printf("Avail to write: %d\n", controlAudio.availableForWrite());
      ++count;
    }
    UsbHostTask();
    //if(micros() - maxTime > 3000) {
    if(micros() - maxTime > 5000) {
      if(count2 < 10) Serial.println("...transfer time out");
      break;
    }
  }
  RESETPROFILEPIN(PROFILER_FT8_CAT_TX);
}

void CheckBlocksToSend() {
  if(t41.RemoteStatus == REMOTE_CONNECTED) {
    if(!BufEmpty() && (controlAudio.availableForWrite() >= 512)) {
      T41ControlSendIQData();
    }
  }
}

void T41ControlBufferIQData(int16_t *pL, int16_t *pR, int block) {
  SETPROFILEPIN(PROFILER_PROCESS_FRAME);

  if(!remoteReady) return;

  if(BufFull()) {
    //Serial.println("*** IQ buffer is full, increase BLOCKS ***");
    // reset buf
    head = tail = 0;
  }

  if(block == 0) {
    // set up sync start
    memcpy(iqBuffer[head], start, BLOCK_SIZE);
    head = (head + 1) & BLOCK_MASK;
  }

  memcpy(iqBuffer[head], pL, 256);
  memcpy(&iqBuffer[head][256], pR, 256);
  head = (head + 1) & BLOCK_MASK;

  if(block == 15) {
    // set up sync end
    memcpy(iqBuffer[head], end, BLOCK_SIZE);
    head = (head + 1) & BLOCK_MASK;

    RESETPROFILEPIN(PROFILER_PROCESS_FRAME);
    // send data
    T41ControlSendIQData();
  }
  RESETPROFILEPIN(PROFILER_PROCESS_FRAME);
}
