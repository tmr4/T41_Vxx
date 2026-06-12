#pragma once

#include <Arduino.h>
#include <Audio.h>

//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

enum ConnectMode { CONNECT_NONE, CONNECT_USB, CONNECT_ETHERNET };

class ConnectBase {
public:
  //ConnectBase() {}

  virtual void init() {};
	virtual void begin() { enabled = true; };
	virtual void end() { enabled = false; }

  virtual bool linkStatus() = 0;
  virtual bool connect() = 0;
  virtual bool connected() = 0;
  virtual void disconnect() {};

  virtual Stream* getCommandStream() = 0;

  virtual ConnectMode getConnectionType() { return CONNECT_NONE; }

  virtual void readToQueue() {}
  virtual void writeToQueue() {}

protected:
  bool enabled = false;
};

template <typename H, typename T>
class ConnectBuffered : public ConnectBase {
public:
  //ConnectBuffered() {}

protected:
	static constexpr size_t maxBlocks = 64; // *** must be power of 2 ***
	static constexpr size_t bufferMask = maxBlocks - 1;
  static_assert((maxBlocks & (maxBlocks - 1)) == 0, "maxBlocks must be a power of 2");

	audio_block_t* volatile queue[maxBlocks][2] = {};
	T tail = 0;
	H head = 0;

  bool bufferFull() { return ((head + 1) & bufferMask) == tail; }
  //bool bufferEmpty() { return head == tail; }
};
