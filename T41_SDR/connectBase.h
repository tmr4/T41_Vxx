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

  virtual bool linkStatus() = 0;
  virtual bool connect() = 0;
  virtual bool connected() = 0;
  virtual void disconnect() {};

  virtual Stream* getStream() = 0;

  virtual ConnectMode getConnectionType() { return CONNECT_NONE; }
};

class EnableBase {
public:
	virtual void begin() { enabled = true; };
	virtual void end() { enabled = false; };

protected:
  bool enabled = false;
};
