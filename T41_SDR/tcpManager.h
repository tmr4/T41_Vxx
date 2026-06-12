#pragma once

/*
 TCPServer and TCPClient - manages Ethernet TCP connections

*/

#include <QNEthernet.h>
using namespace qindesign::network;

//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

class TCPServer {
private:
  EthernetServer server;
  EthernetClient client;

public:
  TCPServer(uint16_t cPort) : cmdPort(cPort) {}

  void begin() {
    initEthernet(serverIP);
    server.begin(cmdPort);
    client.setConnectionTimeoutEnabled(false);
    client.setNoDelay(true);
  }

  void connect() {
    if(!client || !client.connected()) {
      client = server.accept();
    }
  }

  void disconnect() {
    if(Ethernet.linkState()) {
      // just stop if link is still up
      client.stop();
    } else {
      // abort on cable loss
      client.abort();
    }
  }

  bool connected () {
    //return client && client.connected(); // *** I've been using this, but no QNEthernet examples do ***
    return client.connected();
  }

  Stream* getClient() { return &client; }

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

class TCPClient {
private:
  EthernetClient client;

public:
  TCPClient(uint16_t cPort) : cmdPort(cPort) {}

  void begin() {
    initEthernet(clientIP);
    client.setConnectionTimeoutEnabled(false);
    client.setNoDelay(true);
  }

  void connect() {
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
  }

  void disconnect() {
    if(Ethernet.linkState()) {
      // just stop if link is still up
      client.stop();
    } else {
      // abort on cable loss
      client.abort();
    }
  }

  bool connected () {
    return client.connected();
  }

  Stream* getClient() { return &client; }

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
