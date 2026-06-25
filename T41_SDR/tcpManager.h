#pragma once

/*
 TCPServer and TCPClient - manages Ethernet TCP connections

*/

#include <QNEthernet.h>
using namespace qindesign::network;

#include "connectBase.h"

//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

// *** TODO: can simplify ConnectBase and below when connections are separated from streams ***

class TCPServer : public ConnectBase {
private:
  EthernetServer server;
  EthernetClient client;

public:
  TCPServer(uint16_t cPort = 8000) : cmdPort(cPort) {}

  void init() override {
    initEthernet(serverIP);
    server.begin(cmdPort);
    client.setConnectionTimeoutEnabled(false);
    client.setNoDelay(true);
  }

  bool linkStatus() override { return Ethernet.linkState(); }

  bool connect() override {
    if(!client || !client.connected()) {
      client = server.accept();
    }
    return connected();
  }

  void disconnect() override {
    if(Ethernet.linkState()) {
      // just stop if link is still up
      client.stop();
    } else {
      // abort on cable loss
      client.abort();
    }
  }

  bool connected() override {
    //return client && client.connected(); // *** I've been using this, but no QNEthernet examples do ***
    return client.connected();
  }

  Stream* getStream() override { return &client; }
  ConnectMode getConnectionType() override { return CONNECT_ETHERNET; }

private:
  //const IPAddress clientIP{192, 168, 1, 101};
  const IPAddress serverIP{192, 168, 1, 100};
  const IPAddress subnet{255, 255, 255, 0};
  const IPAddress gateway{192, 168, 1, 1};

  uint16_t cmdPort = 0;

  bool isInitialized = false;

  void initEthernet(const IPAddress& ip) {
    if(isInitialized) return; // skip if already started

    Ethernet.begin(ip, subnet, gateway);
    isInitialized = true;
  }
};

class TCPClient : public ConnectBase {
private:
  EthernetClient client;

public:
  TCPClient(uint16_t cPort = 8000) : cmdPort(cPort) {}

  void init() override {
    initEthernet(clientIP);
    client.setConnectionTimeoutEnabled(false);
    client.setNoDelay(true);
  }

  bool linkStatus() override { return Ethernet.linkState(); }

  bool connect() override {
    unsigned long now = millis();

    if(!client.connected() && !client.connecting()) {
      if(now - tcpRetryTimer >= TCP_RETRY_INTERVAL) {
        tcpRetryTimer = now;
        client.abort();
        client.setConnectionTimeoutEnabled(false);
        client.connect(serverIP, cmdPort);
        client.setNoDelay(true);
      }
    }
    return connected();
  }

  void disconnect() override {
    if(Ethernet.linkState()) {
      // just stop if link is still up
      client.stop();
    } else {
      // abort on cable loss
      client.abort();
    }
  }
  bool connected() override { return client.connected(); }

  Stream* getStream() override { return &client; }
  ConnectMode getConnectionType() override { return CONNECT_ETHERNET; }

private:
  const IPAddress clientIP{192, 168, 1, 101};
  const IPAddress serverIP{192, 168, 1, 100};
  const IPAddress subnet{255, 255, 255, 0};
  const IPAddress gateway{192, 168, 1, 1};

  uint16_t cmdPort = 0;

  unsigned long tcpRetryTimer = 0;
  const unsigned long TCP_RETRY_INTERVAL = 2000;

  bool isInitialized = false;

  void initEthernet(const IPAddress& ip) {
    if(isInitialized) return; // skip if already started

    Ethernet.begin(ip, subnet, gateway);
    isInitialized = true;
  }
};
