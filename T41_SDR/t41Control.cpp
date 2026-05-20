
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

#include <QNEthernet.h>
using namespace qindesign::network;
#include "input_tcp.h"
#include "output_tcp.h"

#include "radios.h"

#include "debug.h"

//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

extern RemoteRadio radio;

// for testing
//volatile bool sendGet = false;
bool sendGet = false;

unsigned long lastHeartbeat = 0;

// *** this is display dependent, but also fundamental to much of how the DSP process works ***
#define SPECTRUM_RES          512

bool useKenwoodIF = false;
bool controlDataFlag = false;

// calibration data
bool signalStrengthReceived = false;
float signalStrength = 0.0;
int signalStrengthReceivedIndex = -1;

bool checkingConnection = false;

#if REC_IQ_FROM_T41_ETHER
//EthernetClient ethernetControl;
extern EthernetClient ethernetControl;
extern AudioInputTCP remoteAudioStream;
#endif
#if SEND_IQ_TO_REMOTE_ETHER
//EthernetServer ethernetServerControl(80);
extern EthernetServer ethernetServerControl;
//EthernetClient ethernetControl;
extern EthernetClient ethernetControl;
extern AudioOutputTCP t41AudioStream;
#endif

//-------------------------------------------------------------------------------------------------------------
// Forwards
//-------------------------------------------------------------------------------------------------------------

void SendID(bool request);

//-------------------------------------------------------------------------------------------------------------
// Code
//-------------------------------------------------------------------------------------------------------------

#if REC_IQ_FROM_T41_ETHER || SEND_IQ_TO_REMOTE_ETHER
void InitEthernet(const IPAddress& ip, const IPAddress& subnet, const IPAddress& gateway) {
  static bool isInitialized = false;

  if(isInitialized) return; // Skip if already started

  Ethernet.begin(ip, subnet, gateway);
  //Ethernet.waitForLink(5000);
  //Serial.println(Ethernet.localIP());
  isInitialized = true;
}
#endif

// the three usb serial objects in the teensy (Serial, SerialUSB1 and SerialUSB2) are all different classes (usb_serial_class, usb_serial2_class, and usb_serial3_class)
// I suppose to prevent naming conflict somewhere, but this prevents having serial commands with a common argument specifying the serial channel to use, such as
// void T41ControlSetup(Stream& serial) { serial.begin(); }.  As such might as well duplicate these functions for both the T41 control app and Beacon monitor
void T41ControlSetupOld() {
  //controlSerial.begin(19200);
  // *** this controls whether debug messages go out over Serial ***
  // *** this is needed for USB where the remote Serial connection is used for CAT control ***
  // *** that's not the case for Ethernet ***
  // *** TODO: fix for Ethernet if needed ***
  if(CAT_CONTROL_REMOTE) {
    //sendGet = true;
    sendGet = false;
  } else {
    //sendGet = true;
    sendGet = false;
  }
#if REC_IQ_FROM_T41_ETHER || SEND_IQ_TO_REMOTE_ETHER
  const IPAddress clientIP{192, 168, 1, 101};
  const IPAddress subnet{255, 255, 255, 0};
  const IPAddress gateway{192, 168, 1, 1};
  const IPAddress serverIP{192, 168, 1, 100};
#if REC_IQ_FROM_T41_ETHER
  // Remote Ethernet Client
  InitEthernet(clientIP, subnet, gateway);
#elif SEND_IQ_TO_REMOTE_ETHER
  // T41 Ethernet Server
  InitEthernet(serverIP, subnet, gateway);
  ethernetServerControl.begin(80);
#endif
  ethernetControl.setConnectionTimeoutEnabled(false);
  ethernetControl.setNoDelay(true);
#endif
}

void T41RemoteConnectCheck() {
  static unsigned long last = 0;
  unsigned long now = millis();
  int lasped = now - last;
  int remoteStatus = t41.RemoteStatus;
  bool connected = t41.RemoteStatus == REMOTE_CONNECTED;
  static bool wasConnected = false;
  static bool connectionLost = false;

  // T41 sends a heartbeat to remote, remote replies
  // Remote knows it's connected if it receives the heartbeat or a command every 15s
  // T41 knows it's connected if it receives response from remote every 15s
  #if SEND_IQ_TO_REMOTE_USB
  // send a connection request every 5s until connected
  if(!connected) {
    if(lasped > 5000) {
      SendID(true);
      last = now;
    }
  }
  #endif
  #if SEND_IQ_TO_REMOTE_USB || SEND_IQ_TO_REMOTE_ETHER
  // send a heartbeat every 15s
  if(wasConnected) {
    if(lasped > 15000) {
      SendID(true);
      last = now;
    }
  }
  #endif

  //if(wasConnected) {
  //  if(millis() - lastHeartbeat > 30000) {
  //    remoteStatus = REMOTE_LOST;
  //    connected = false;
  //  } else {
  //    remoteStatus = REMOTE_CONNECTED;
  //    connected = true;
  //  }
  //} else {
  //  if(millis() - lastHeartbeat > 10000) {
  //    remoteStatus = REMOTE_WAITING;
  //    connected = false;
  //  } else {
  //    remoteStatus = REMOTE_CONNECTED;
  //    connected = true;
  //  }
  //}

  // *** TODO: consider something similar for Ethernet, but it seems more robust than USB ***
  #if REC_IQ_FROM_T41_USB
  // *** remote can detect dtr on connect but doesn't catch cable disconnect ***
  // DTR is a reliable indicator that Serial has connected to a host
  // *** it is not a reliable indicator of a disconnect ***
  // *** !Serial is not a reliable indicator of a disconnect ***
  connected = Serial.dtr();
  if(connected) {
    remoteAudioStream.begin();
  } else {
    remoteAudioStream.end();
  }
  #endif

  #if REC_IQ_FROM_T41_ETHER || SEND_IQ_TO_REMOTE_ETHER
  //Serial.println("checking connection");
  //if(!connected || !ethernetControl.connected()) {
  if(!connected && (lasped > 5000)) {
    last = now;
    //Serial.println("not connected");
    const IPAddress serverIP{192, 168, 1, 100};

    // try to connect
    ethernetControl.stop();
    //ethernetControl.abort();
    ethernetControl.setConnectionTimeoutEnabled(false);
    ethernetControl.setNoDelay(true);
    #if REC_IQ_FROM_T41_ETHER
    if(ethernetControl.connect(serverIP, 80) == 1) connected = true;
    #elif SEND_IQ_TO_REMOTE_ETHER
    ethernetControl = ethernetServerControl.accept();
    if(ethernetControl) {
      connected = true;
    }
    #endif
  }
  #endif

  // show remote connection status
  if(connected) {
    if(!wasConnected) {
      wasConnected = true;
    }
    remoteStatus = REMOTE_CONNECTED;
  } else {
    if(wasConnected) {
      if(connectionLost) {
        remoteStatus = REMOTE_WAITING;
        connectionLost = false;
      } else {
        remoteStatus = REMOTE_LOST;
        connectionLost = true;
      }
    } else {
      remoteStatus = REMOTE_WAITING;
    }
  }

  t41.RemoteStatus = remoteStatus;
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
  if(sendGet) {
    SETPROFILEPIN(PROFILER_FT8_CAT_TX);
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
/*
void TogglePins() {
  static int count = 0;
  for(int i = 0; i < 10; i++) {
    switch(count) {
      case 0:
        TOGGLEPROFILEPIN(PROFILER_MAINLOOP);
        break;
      case 1:
        TOGGLEPROFILEPIN(PROFILER_PROCESS_RX);
        break;
      case 2:
        TOGGLEPROFILEPIN(PROFILER_DRAWFREQSPEC);
        break;
      case 3:
        TOGGLEPROFILEPIN(PROFILER_DRAWAUDIOSPEC);
        break;
      case 4:
        TOGGLEPROFILEPIN(PROFILER_PROCESS_FRAME);
        break;
      case 5:
        TOGGLEPROFILEPIN(PROFILER_FT8_REMOTE_RX);
        break;
      case 6:
        TOGGLEPROFILEPIN(PROFILER_DECODE_FT8);
        break;
      case 7:
        TOGGLEPROFILEPIN(PROFILER_FT8_CAT_TX);
        break;
    }
    delay(1);
  }
  ++count;
  if(count >= 8) count = 0;
}
*/
// Dual T41 master commands
// for sending integer-based commands between T41 and remote
void SendCommand(int value, int id) {
  char cmd[30]; // 50 if we include IF

  switch(id) {
    case T41_ITEM_VOL:
      //TogglePins();
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
  //T41ControlSendCmd(cmd);
  radio.SendCommand(cmd);
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
    lastHeartbeat = millis();


    if(sendGet) {
      SETPROFILEPIN(PROFILER_FT8_REMOTE_RX);
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
          //t41.RemoteStatus = REMOTE_CONNECTED;
        } else if(cmd[1] == 'D' && cmd[5] == ';') { // IDxxx;
          if(checkingConnection) {
            checkingConnection = false;
          }
          #if SEND_IQ_TO_REMOTE_USB
          t41.RemoteStatus = REMOTE_CONNECTED;
          #endif
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

void SendMsg(const char *msg, int value) {
  char cmd[256];
  sprintf(cmd, msg, value);
  T41ControlSendCmd(cmd);
}
