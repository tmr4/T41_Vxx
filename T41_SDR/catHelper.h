#pragma once

#include <Stream.h>

//-------------------------------------------------------------------------------------------------------------
// Forwards
//-------------------------------------------------------------------------------------------------------------

class CatControl;
class T41Update;

//-------------------------------------------------------------------------------------------------------------
// Helpers
//-------------------------------------------------------------------------------------------------------------

struct CATAction {
  virtual void execute(CatControl* instance, const char* cmd) const = 0;
};

struct CATCommand {
  const uint16_t token;             // "xx"_cat
  const T41Update* readProperty;    //
  const T41Update* actnProperty;    //
  const char* const format;         // answer format
  const CATAction* const action;    // set function
  const uint8_t lenR;               // read command length
  const uint8_t lenS;               // set command length
};

// macros to create the CAT command table items into PROGMEM
//    DEFINE_CAT_COMMAND:
//    DEFINE_CAT_CMD_PROP:
//    DEFINE_CAT_CMD_ACTN:


// *** use of __attribute__((section(".progmem.data") vs PROGMEM resolves SetFunctionName##_Wrapper
//     section type conflict with catCommands set as PROGMEM or can use PROGMEM here and use
//     __attribute__((section(".progmem.data") on catCommands ***
//#define DEFINE_CAT_CMD_PROP(Token, ReadPropertyIndex, SetFunctionName, AnswerFormatStr, ReadLen, SetLen)
#define DEFINE_CAT_CMD_PROP(Token, ReadProperty, SetFunctionName, AnswerFormatStr, ReadLen, SetLen) \
  /* 1. Flash string: fmt_cat_FA */ \
  static inline const char fmt_##SetFunctionName[] PROGMEM = AnswerFormatStr; \
  \
  /* 2. Wrapper: Action_cat_FA */ \
  struct Action_##SetFunctionName : public CATAction { \
    void execute(CatControl* instance, const char* cmd) const override { \
      SetFunctionName(instance, cmd); \
    } \
  }; \
  static inline const Action_##SetFunctionName SetFunctionName##_Wrapper PROGMEM = {}; \
  \
  /* 3. Flash Struct: cat_FA_cmd */ \
  static inline const CATCommand SetFunctionName##_cmd PROGMEM = { \
    Token, \
    ReadProperty, \
    nullptr, \
    fmt_##SetFunctionName, \
    &SetFunctionName##_Wrapper, \
    ReadLen, \
    SetLen \
  }

#define DEFINE_CAT_CMD_ACTN(Token, ActionPropertyIndex, SetFunctionName, AnswerFormatStr, ReadLen, SetLen) \
  /* 1. Flash string: fmt_cat_FA */ \
  static inline const char fmt_##SetFunctionName[] PROGMEM = AnswerFormatStr; \
  \
  /* 2. Wrapper: Action_cat_FA */ \
  struct Action_##SetFunctionName : public CATAction { \
    void execute(CatControl* instance, const char* cmd) const override { \
      SetFunctionName(instance, cmd); \
    } \
  }; \
  static inline const Action_##SetFunctionName SetFunctionName##_Wrapper PROGMEM = {}; \
  \
  /* 3. Flash Struct: cat_FA_cmd */ \
  static inline const CATCommand SetFunctionName##_cmd PROGMEM = { \
    Token, \
    nullptr, \
    nullptr, \
    fmt_##SetFunctionName, \
    &SetFunctionName##_Wrapper, \
    ReadLen, \
    SetLen \
  }

#define DEFINE_CAT_COMMAND(Token, SetFunctionName, AnswerFormatStr, ReadLen, SetLen) \
  /* 1. Flash string: fmt_cat_FA */ \
  static inline const char fmt_##SetFunctionName[] PROGMEM = AnswerFormatStr; \
  \
  /* 2. Wrapper: Action_cat_FA */ \
  struct Action_##SetFunctionName : public CATAction { \
    void execute(CatControl* instance, const char* cmd) const override { \
      SetFunctionName(instance, cmd); \
    } \
  }; \
  static inline const Action_##SetFunctionName SetFunctionName##_Wrapper PROGMEM = {}; \
  \
  /* 3. Flash Struct: cat_FA_cmd */ \
  static inline const CATCommand SetFunctionName##_cmd PROGMEM = { \
    Token, \
    nullptr, \
    nullptr, \
    fmt_##SetFunctionName, \
    &SetFunctionName##_Wrapper, \
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

#define CAT_HASH_CONSTANT 40305U
#define CAT_TOKEN_TO_HASH ((token * CAT_HASH_CONSTANT) & 0xFFFF) >> 9 // create 128-slot hash index
constexpr uint8_t operator "" _cath(const char* str, size_t len) {
  uint16_t token = (len < 2) ? 0 : (static_cast<uint16_t>(str[0]) << 8) | static_cast<uint16_t>(str[1]);
  return CAT_TOKEN_TO_HASH;
}

inline uint8_t CatToken2Hash(uint16_t token) {
  return CAT_TOKEN_TO_HASH;
}
