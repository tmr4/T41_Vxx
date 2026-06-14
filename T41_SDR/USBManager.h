#pragma once

#include <USBHost_t36.h>

class USBManager {
private:
  static USBHost usbHost;
  static USBHub usbHub;

public:
  static inline USBHost& getHost() { return usbHost; }
  static inline void begin() { getHost().begin(); }
};
