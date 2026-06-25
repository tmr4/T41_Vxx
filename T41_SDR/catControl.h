#pragma once

#include <Stream.h>

#include "catHelper.h"
#include "connectBase.h"
#include "USBManager.h"

//-------------------------------------------------------------------------------------------------------------
// Forwards
//-------------------------------------------------------------------------------------------------------------

class CatControl;

void SendCommand(int id);
#if CAT_SPY
void catSpy(const char* cmd, int type);
#else
inline void catSpy(const char* cmd, int type) {}
#endif

//-------------------------------------------------------------------------------------------------------------
// Helpers
//-------------------------------------------------------------------------------------------------------------

//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

/*

CatControl creates the framework for CAT control support. It provides the following public methods:

  getStream: sets a pointer to the Stream derived communication object

  update: checks for and processes available CAT commands according to command table provided by child class (see radio.h)
          *** update must be called frequently to check for available commands         ***
          *** calls to update must observe any restrictions of the Stream object used. ***
          *** For example, update can't be call from an interrupt for an Ethernet      ***
          *** Stream object.                                                           ***

  notifyRemote: processes CAT command associated with specific item in catItems array
                *** handy for notifying remote unit about changes to radio properties ***

*/

class CatControl {
private:
  const CATCommand* const *catCommands;

  static constexpr uint8_t maxCmd = 255;
  static constexpr uint8_t maxMsg = 50; // should be enough to respond to IF;
  static constexpr uint8_t timeout = 250;

public:
  CatControl(const CATCommand* const *cmds) : catCommands(cmds), useWSJT(false) {}
  CatControl(const CATCommand* const *cmds, Stream* s) : catCommands(cmds), enabled(true), stream(s), useWSJT(true) {}
  virtual ~CatControl() {}

  void begin() { enabled = true; }
	void end() { enabled = false; }

  void setConnectBase(ConnectBase* cb, bool rt = false) {
    connectBase = cb;
    runTask = rt;
    enabled = true;
  }

  void update() {
    if(enabled) {
      getStream();
      if(!stream) return;

      // timeout
      if(idx > 0 && (millis() - lastCharTime > timeout)) idx = 0;

      while(stream->available()) {
        char c = stream->read();
        lastCharTime = millis();

        if(c == ';') {
          cmd[idx++] = ';';
          cmd[idx] = '\0'; // terminate it
          processCommand(cmd);
          idx = 0;
        } else if(idx < maxCmd - 1) { // leave room for ';' and '\0'
          cmd[idx++] = c;
        } else {
          idx = 0;
        }
      }
    }
  }

  // notify remote by of a change by inserting the associated
  // CAT command into processCommand
  void notifyRemote(int token) {
    if(enabled) {
      char cmd[4] = "xx;";

      // convert token into CAT command
      cmd[0] = static_cast<char>(((uint16_t)token & 0xFF00) >> 8);
      cmd[1] = static_cast<char>((uint16_t)token & 0xFF);
      //Serial.printf("Notify remote: %s\n", cmd);
      processCommand(cmd);
    }
  }

  unsigned long getHeartbeat() { return heartbeat; }
  void setHeartbeat(unsigned long hb) { heartbeat = hb; }
  bool isWSJT() { return useWSJT; }

  virtual void ackIdReceipt() {}

  void send(const char *msg) {
    if(enabled) {
      //Serial.printf("Sent: %s\n", msg);
      getStream();
      if(stream && stream->availableForWrite() > 50) {
        if(useWSJT) catSpy(msg, 0);
        stream->print(msg);
      }
    }
  }

protected:
  bool enabled = false;

  ConnectBase* connectBase = nullptr;
  Stream* stream = nullptr;
  bool runTask = false;

  char cmd[maxCmd + 1]; // leave room for terminating null
  char msg[maxMsg + 1]; // leave room for terminating null
  uint8_t idx = 0;
  unsigned long lastCharTime = 0;
  bool useWSJT = false;

  void processCommand(const char* cmd);

  void getStream() {
    if(useWSJT) {
      return;
    } else if(connectBase && connectBase->connected()) {
      if(runTask) USBManager::getHost().Task();
      stream = connectBase->getStream();
    } else {
      stream = nullptr;
    }
  }

  void HandleNonstandardProperty(const CATCommand* item);

protected:
  unsigned long heartbeat = 0;
};
