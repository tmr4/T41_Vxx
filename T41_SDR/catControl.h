
#include <Stream.h>

//-------------------------------------------------------------------------------------------------------------
// Forwards
//-------------------------------------------------------------------------------------------------------------

void T41RemoteConnectCheck();

//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

void SendCommand(int id);

class CatControl;

struct CATAction {
  virtual void execute(CatControl* instance, const char* cmd, bool read) const = 0;
};

struct CATCommand {
  const uint16_t code;  // packed integer of 2 character CAT command (see operator "" _cat below)
  const uint8_t lenR, lenS;
  const CATAction& action;
};

#define DEFINE_CAT_ACTION(ClassName, MethodName) \
  struct Action_##MethodName : public CATAction { \
    void execute(CatControl* parentPtr, const char* cmd, bool isRead) const override { \
      static_cast<ClassName*>(parentPtr)->MethodName(cmd, isRead); \
    } \
  }; \
  static inline Action_##MethodName MethodName##_Wrapper;

class CatControl : public Stream {
private:
  const CATCommand* const commands;
  const size_t numCmds;
  static const uint16_t catItems[T41_ITEMS];

  static constexpr uint8_t maxCmd = 255;
  static constexpr uint8_t maxMsg = 50; // should be enough to respond to IF;
  static constexpr uint8_t timeout = 250;

public:
  CatControl(const CATCommand* const cmds, size_t size) : commands(cmds), numCmds(size) {}
  virtual ~CatControl() {}

  void setLink(Stream& s) { link = &s; }

  int available() override { if(link) return link->available(); else return 0; }
  int read() override { if(link) return link->read(); else return -1; }
  int peek() override { if(link) return link->peek(); else return -1; }
  size_t write(uint8_t c) { if(link) return link->write(c); else return -1;}

  void update() {
    if(!link) return;

    T41RemoteConnectCheck();

    // timeout
    if(idx > 0 && (millis() - lastCharTime > timeout)) idx = 0;

    while(link->available()) {
      char c = link->read();
      lastCharTime = millis();

      if(c == ';') {
        cmd[idx++] = ';';
        cmd[idx] = '\0';
        processCommand(cmd);
        idx = 0;
      } else if(idx < maxCmd - 1) { // leave room for ';' and '\0'
        cmd[idx++] = c;
      } else {
        idx = 0;
      }
    }
  }

  void NotifyRemote(int item) {
    uint16_t cat = catItems[item];
    char cmd[4] = "xx;";

    cmd[0] = static_cast<char>((cat & 0xFF00) >> 8);
    cmd[1] = static_cast<char>(cat & 0xFF);
    processCommand(cmd);
  }

protected:
  Stream* link = nullptr;
  char cmd[maxCmd + 1];
  char msg[maxMsg + 1];
  uint8_t idx = 0;
  unsigned long lastCharTime = 0;

  void processCommand(const char* cmd) {
    // convert the 2 character command code into a single uint16_t
    uint16_t cmdCode = (static_cast<uint16_t>(cmd[0]) << 8) | static_cast<uint16_t>(cmd[1]);

    // look for a match in dispatch table
    for(size_t i = 0; i < numCmds; i++) {
      const CATCommand& item = commands[i];

      if(cmdCode == item.code) {
        // CAT command found
        bool isRead = false;

        if(item.lenR != 0 && cmd[item.lenR-1] == ';') {
          // read command properly formed
          isRead = true;
        } else if(item.lenS != 0 && cmd[item.lenS-1] == ';') {
          // set command properly formed
          isRead = false;
        } else {
          // command not properly formed
          // *** TODO: consider sending followup if command not properly formed
          return;
        }
        item.action.execute(this, cmd, isRead);
        if(isRead) send(msg);
        return;
      }
    }
    // *** TODO: consider sending ?; if command not recognized
  }

  // *** TODO: consider this against T41ControlSendMsg and if this should be virtual ***
  // *** can skip link null check as long as this is called only from update
  //void send(const char *msg) { if(link) link->print(msg); }
  void send(const char *msg);

  virtual void handleID(const char* cmd, bool isRead);
  virtual void ackIdReceipt() {}

  // handlers common to all radios
  void handleBD(const char* cmd, bool isRead);
  void handleBU(const char* cmd, bool isRead);
  void handleFA(const char* cmd, bool isRead);
  void handleFB(const char* cmd, bool isRead);
  void handleFC(const char* cmd, bool isRead);
  void handleTM(const char* cmd, bool isRead);
};

//-------------------------------------------------------------------------------------------------------------
// Code
//-------------------------------------------------------------------------------------------------------------

// convert 2 character CAT commands into a uint16_t
constexpr uint16_t operator "" _cat(const char* str, size_t len) {
    return (static_cast<uint16_t>(str[0]) << 8) | static_cast<uint16_t>(str[1]);
}

void T41ControlSetup();

/*
// common commands for adding to radio specific dispatch tables
{
  {"BD"_cat, , , handleBD},       // band down
  {"BU"_cat, , , handleBU},         // band up
  {"FA"_cat, , , handleFA},             // read/set VFO A frequency
  {"FB"_cat, , , handleFB},             // read/set VFO B frequency
  {"FC"_cat, , , handleFC},             // read/set current VFO center frequency
  {"ID"_cat, , , handleID},             // read radio ID
  {"TM"_cat, 0, 14, handleTM_Wrapper},   // set Teensy RTC
};

*/
