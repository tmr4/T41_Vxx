#pragma once

#include <Arduino.h>
#include <Audio.h>

//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

enum ConnectMode { CONNECT_NONE, CONNECT_USB, CONNECT_ETHERNET };

class ConnectBase {
public:
  virtual void init() = 0;
	virtual void begin() = 0;
	virtual void end() = 0;

  virtual bool linkStatus() = 0;
  virtual bool connect() = 0;
  virtual bool connected() = 0;
  virtual void disconnect() {};

  virtual Stream* getCommandStream() = 0;

  virtual ConnectMode getConnectionType() { return CONNECT_NONE; }

  virtual void readToQueue() {}
  virtual void writeToQueue() {}
};
