
#include <Stream.h>

//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

class CatControl : public Stream {
private:
  static constexpr uint8_t maxCmd = 255;
  static constexpr uint8_t maxMsg = 50; // should be enough to respond to IF;
  static constexpr uint8_t timeout = 250;

protected:
  typedef void (CatControl::*CmdHandler)(const char*, const size_t len);

  Stream* link = nullptr;
  char cmd[maxCmd + 1];
  char msg[maxMsg + 1];
  uint8_t idx = 0;
  unsigned long lastCharTime = 0;

  struct CommandEntry {
      const char cmd[3];    // e.g., "FA"
      CmdHandler handler;   // function to call
  };

  virtual const CommandEntry* getDispatchTable() = 0;
  virtual size_t getTableSize() = 0;

  void handleCommand(const char* cmd) {
    const CommandEntry* table = getDispatchTable();
    size_t size = getTableSize();

    // look for a match in dispatch table
    for(size_t i = 0; i < size; i++) {
      if(strncmp(cmd, table[i].cmd, 2) == 0) {
        size_t len = strlen(cmd);
        if(cmd[len-1] == ';') {
          // command found and properly formed (ends with ;)
          (this->*(table[i].handler))(cmd, len);
        }
        return;
      }
    }
    // *** TODO: consider sending ?; if command not recognized
  }

public:
  CatControl() {}
  virtual ~CatControl() {}

  void setLink(Stream& s) { link = &s; }

protected:
  // *** TODO: consider this against T41ControlSendMsg and if this should be virtual ***
  // *** can skip link null check as long as this is called only from update
  //void send(const char *msg) { if(link) link->print(msg); }
  void send(const char *msg) { link->print(msg); }

  void update() {
    if(!link) return;

    // timeout
    if(idx > 0 && (millis() - lastCharTime > timeout)) idx = 0;

    while(link->available()) {
      char c = link->read();
      lastCharTime = millis();

      if(c == ';') {
          buffer[idx] = '\0';
          handleCommand(cmd);
          idx = 0;
      } else if(idx < maxBuf) {
          cmd[idx++] = c;
      }
    }
  }

  virtual void handleID(const char* data, const size_t len);
  virtual void ackIdReceipt() {}

private:
  // handlers common to all radios
  void handleBD(const char* data, const size_t len);
  void handleBU(const char* data, const size_t len);
  void handleFA(const char* data, const size_t len);
  void handleFB(const char* data, const size_t len);
  void handleFC(const char* data, const size_t len);
};

//-------------------------------------------------------------------------------------------------------------
// Code
//-------------------------------------------------------------------------------------------------------------

/*
// common commands for adding to radio specific dispatch tables
{
  {"BD", (CatControl::CmdHandler)&(specific class)::handleBandDown},       // band down
  {"BU", (CatControl::CmdHandler)&(specific class)::handleBandUp},         // band up
  {"FA", (CatControl::CmdHandler)&(specific class)::handleFA},             // read/set VFO A frequency
  {"FB", (CatControl::CmdHandler)&(specific class)::handleFB},             // read/set VFO B frequency
  {"FC", (CatControl::CmdHandler)&(specific class)::handleFC},             // read/set current VFO center frequency
  {"ID", (CatControl::CmdHandler)&(specific class)::handleID},             // read radio ID
};

*/
