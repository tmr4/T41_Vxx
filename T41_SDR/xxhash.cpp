#include <Arduino.h>

void GenerateStartEndSyncBlock(uint8_t *buf, uint64_t salt) {
  const uint64_t seed = 0x9E3779B97F4A7C15ULL; // fractional part of the Golden Ratio (2^64 / phi)
  uint64_t blockData;
  uint64_t value = seed + salt;

  for (int i = 0; i < 256; i += 8) {
    // simple Xorshift-style transform
    value ^= (value << 13);
    value ^= (value >> 7);
    value ^= (value << 17);
    blockData = value + seed;

    memcpy(buf + i, &blockData, 8);
  }
}

uint64_t IQQuickHash(uint8_t *buf) {
  uint64_t h = 0;

  h ^= *(uint64_t*)(buf + 0);
  h ^= *(uint64_t*)(buf + 64);
  h ^= *(uint64_t*)(buf + 128);
  h ^= *(uint64_t*)(buf + 192);

  return h;
 }
