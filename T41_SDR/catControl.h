#pragma once

#include <Stream.h>

//-------------------------------------------------------------------------------------------------------------
// Forwards
//-------------------------------------------------------------------------------------------------------------

class CatControl;

void SendCommand(int id);

//-------------------------------------------------------------------------------------------------------------
// Helpers
//-------------------------------------------------------------------------------------------------------------

struct CATAction {
  virtual void execute(CatControl* instance, const char* cmd) const = 0;
};

struct CATCommand {
  const uint16_t token;
  const char* const format;       // answer format
  //const CATAction* action;
  const CATAction* const action;
  const uint8_t lenR; // read/set command length
  const uint8_t lenS;
};

// macro to create the methods needed get the CAT command table into PROGMEM
// *** use of __attribute__((section(".progmem.data") vs PROGMEM resolves MethodName##_Wrapper
//     section type conflict with catCommands set as PROGMEM or can use PROGMEM here and use
//     __attribute__((section(".progmem.data") on catCommands ***
#define DEFINE_CAT_COMMAND(ClassName, MethodName, Token, FormatStr, ReadLen, SetLen) \
  /* 1. Flash string: fmt_cat_FA */ \
  static inline const char fmt_##MethodName[] PROGMEM = FormatStr; \
  \
  /* 2. Wrapper: Action_cat_FA */ \
  struct Action_##MethodName : public CATAction { \
    void execute(CatControl* parentPtr, const char* cmd) const override { \
      static_cast<ClassName*>(parentPtr)->MethodName(cmd); \
    } \
  }; \
  static inline const Action_##MethodName MethodName##_Wrapper PROGMEM = {}; \
  \
  /* 3. Flash Struct: cat_FA_cmd */ \
  static inline const CATCommand MethodName##_cmd PROGMEM = { \
      Token, \
      fmt_##MethodName, \
      &MethodName##_Wrapper, \
      ReadLen, \
      SetLen \
  }

// convert 2 character CAT command into a unique uint16_t identifier
constexpr uint16_t operator "" _cat(const char* str, size_t len) {
  return (len < 2) ? 0 : (static_cast<uint16_t>(str[0]) << 8) | static_cast<uint16_t>(str[1]);
}

// convert 2 character CAT commands into a uint8_t index
// https://probablydance.com/2018/06/16/fibonacci-hashing-the-optimization-that-the-world-forgot-or-a-better-alternative-to-integer-modulo/
// uses a custom constant (AI generated python script) that results in zero collisions for both CAT tables (catCommands and wsjtCommands)
// The clasic 16-bit golden ratio, 40503, results in 5 collision clusters of 7 commands total out of the main
// 30 command CAT table
/*
Index   Colliding Commands
13      BD, FA
45      BU, F0, ID
3       DP, NS
40      NG, TM
19      FF, VO

**************************************

Python script to find collision free constant:
def find_best_constant(commands, table_size=128):
  # Calculate shift for 16-bit input (e.g., 16-6 = 10 for size 64, or 16-7 = 9 for size 128)
  bits = (table_size - 1).bit_length()
  shift = 16 - bits

  # The ideal 16-bit Golden Ratio constant (2^16 / 1.61803...)
  phi_16 = 40503

  # Search outward from the Golden Ratio to find a perfect hash
  # that retains the best scattering properties.
  for offset in range(0, 32768):
    for sign in [1, -1]:
      constant = (phi_16 + (sign * offset)) & 0xFFFF

      # Multiplier must be odd to preserve all bits of the input
      if constant % 2 == 0: continue

      indices = set()
      collision = False
      for cmd in commands:
        token = (ord(cmd[0]) << 8) | ord(cmd[1])
        index = ((token * constant) & 0xFFFF) >> shift

        if index in indices:
          collision = True
          break
        indices[index] = cmd

      if not collision:
        return constant
  return None

# Your set of 30 commands
cmds = ["BD","BU","DP","DS","FA","FB","FC","FF","FS","FT","F0","F1","GT","ID","IF",
        "MD","ME","NF","NG","NH","NL","NS","NW","N1","PC","PG","SM","TM","VO","ZM"]

best_c = find_best_constant(cmds)
print(f"Perfect Constant found near Golden Ratio: {best_c}")

**************************************

Distribution for two constants:

def get_map(indices, table_size=128   ):
  table = ["." for _ in range(table_size)]
  for idx in indices:
    table[idx] = "X"
  print(f"Distribution Map (128 slots):\n|{''.join(table)}|")

# 36 unique commands
all_cmds = ["BD","BU","DP","DS","FA","FB","FC","FF","FS","FT","F0","F1","GT","ID","IF",
            "MD","ME","NF","NG","NH","NL","NS","NW","N1","PC","PG","SM","TM","VO","ZM",
            "AI","KS","SF","SP","TB","TX"]

constant, mapping = find_best_constant(all_cmds)

if constant:
  print(f"New Perfect Constant: {constant}")
  get_map(mapping)
else:
  print("No perfect constant found. Consider increasing table size.")

Visual Distribution Map (64-slot table)
Constant: 13561 (Smaller, arbitrary perfect hash)
Constant: 38617 (Golden Ratio Centered)

Each X represents a filled slot in your 64-entry table, '.' represents empty slots.

Index:            1         2         3         4         5         6
        0123456789012345678901234567890123456789012345678901234567890123
        |         |         |         |         |         |         |
13561:  XXX.X..X...X.XX.XXX.XXXX....XXXXX.XXXX..X.XXX..X.XX....X..X.XX.X
38617:  XXXX.X.XX....XXX..X..X.XX...X.XX..XXXXXXXXXX..X..X..XX...X.XXX.X

Observations: Notice the larger gaps (e.g., between index 7 and 11, or 50 and 55).
There is a dense cluster of 5 commands in the middle (indices 28-32).

Observations: This distribution is more "fragmented." While it has a central sequence,
 the gaps are generally smaller and more frequent. This is the discrepancy property of
  the Golden Ratio at work; it tries to "fill the gaps" more aggressively.

128-slot table:
Index:            1         2         3         4         5         6         7         8         9         0         1         2
        01234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567
40305:  X...XX.X..X.XX.....X.X...X.X......X.................XXXX....XXX...X..X.XX..X.........X.X..X............X...X...X.......XXXX..XX.

*/

constexpr uint8_t operator "" _cath(const char* str, size_t len) {
  uint16_t token = (len < 2) ? 0 : (static_cast<uint16_t>(str[0]) << 8) | static_cast<uint16_t>(str[1]);
  // create 128-slot hash index
  return ((token * 40305U) & 0xFFFF) >> 9;
}

//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

/*

CatControl creates the framework for CAT control support. It provides the following public methods:

  setLink: sets a pointer to the Stream derived communication object

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
  const CATCommand* const *commands;
  static const uint16_t catItems[T41_ITEMS];

  static constexpr uint8_t maxCmd = 255;
  static constexpr uint8_t maxMsg = 50; // should be enough to respond to IF;
  static constexpr uint8_t timeout = 250;

public:
  CatControl(const CATCommand* const *cmds, bool wsjt) : commands(cmds), useWSJT(wsjt) {}
  virtual ~CatControl() {}

  void setLink(Stream& s) { link = &s; }

  void update() {
    if(!link) return;

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

  void notifyRemote(int item) {
    uint16_t cat = catItems[item];
    char cmd[4] = "xx;";

    cmd[0] = static_cast<char>((cat & 0xFF00) >> 8);
    cmd[1] = static_cast<char>(cat & 0xFF);
    processCommand(cmd);
  }

protected:
  Stream* link = nullptr;
  char cmd[maxCmd + 1]; // leave room for terminating null
  char msg[maxMsg + 1]; // leave room for terminating null
  uint8_t idx = 0;
  unsigned long lastCharTime = 0;
  bool useWSJT = false;

  void processCommand(const char* cmd) {
    // convert the 2 character command code into a single uint16_t
    uint16_t cmdCode = (uint16_t)((cmd[0] << 8) | cmd[1]);
    uint8_t cmdIndex = (uint16_t)(cmdCode * 40305U) >> 9;
    const CATCommand* item = commands[cmdIndex];

    if(item) {
      // CAT command found
      if(item->lenR != 0 && cmd[item->lenR-1] == ';') {
        // read command properly formed
        snprintf(msg, sizeof(msg), item->format, GetPropertyValue(item->token));
        send(msg);
      } else if(item->lenS != 0 && cmd[item->lenS-1] == ';') {
        // set command properly formed
        item->action->execute(this, cmd);
      } else {
        // command not properly formed
        // *** TODO: consider sending followup if command not properly formed
        return;
      }
    }
    // *** TODO: consider sending ?; if command not recognized
  }

  // *** TODO: consider this against T41ControlSendMsg and if this should be virtual ***
  // *** can skip link null check as long as this is called only from update
  void send(const char *msg) { if(link) link->print(msg); }
  //void send(const char *msg);

  int GetPropertyValue(int token);

protected:
  unsigned long heatbeart = 0;

  friend class ConnectManager;
};
