#pragma once

#include <Arduino.h>
#include <USBHost_t36.h>

#include <QNEthernet.h> // https://github.com/ssilverman/QNEthernet
using namespace qindesign::network;

#include "AudioConfig.h"
#include "catControl.h"
#include "t41Property.h"

/*

  I'm still determining the direction of this class. The intent is to allow
  for various connection types, but that might not be a good use of Teensy RAM

  Currently connection is hardcoded for Ethernet: TCP for command channel and UDP for data

*/

//-------------------------------------------------------------------------------------------------------------
// Forward
//-------------------------------------------------------------------------------------------------------------

void SetupRemoteIQStream(ConnectMode connectMode);

//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

extern "C" struct netif* netif_default;

class USBManager {
private:
  static USBHost usbHost;
  static USBHub usbHub;

public:
  //USBManager();

  static inline USBHost& getHost() { return usbHost; }
  static inline void begin() { getHost().begin(); }
};

enum DeviceRole { REMOTE_ROLE_T41, REMOTE_ROLE_REMOTE };
enum LinkState { LINK_DISCONNECTED, LINK_CHECK_CONNECTION, LINK_CHECK_HEARTBEAT, LINK_CONNECTED, LINK_LOST };

// Handles USB or Ethernet connection state for remote audio / CAT control
// Remote IQ data stream: The T41 IQ data stream is transfered to a remote unit over USB Host/Ethernet.
// The remote unit receives the data on USB serial/Ethernet. The specific objects are declared globally
// based on the mode selected in the hardware config file, hardwareConfig.h, for each unit and passed
// by pointer to their common ConnectBase class in the constructor.
class ConnectManager {
private:
  ConnectBase* activeConnection = nullptr;
  ConnectBase* usbConnection = nullptr;
  ConnectBase* ethernetConnection = nullptr;

public:
  ConnectManager(DeviceRole _role = REMOTE_ROLE_T41) : role(_role) {}

  //void switchConnection(ConnectBase* connection) {
  //  if(activeConnection) activeInterface->stop();
  //  activeConnection = connection;
  //  activeConnection->begin();
  //
  //  Stream* cmdStream = activeConnection->getCommandStream();
  //  catControl.setStream(cmdStream);
  //}

private:
  DeviceRole role = REMOTE_ROLE_T41;
  LinkState linkState = LINK_DISCONNECTED;

  const unsigned long POLL_INTERVAL = 40;
  //const unsigned long HEARTBEAT_INTERVAL = 15000;
  const unsigned long HEARTBEAT_INTERVAL = 2000;
  //const unsigned long HEARTBEAT_INTERVAL = 1000;
  //const unsigned long HEARTBEAT_INTERVAL = 500;
  const unsigned long HEARTBEAT_TIMEOUT = (HEARTBEAT_INTERVAL * 3);
  const int HEARTBEAT_COUNT = 2;

public:
  void begin(CatControl *control, ConnectBase* udp, ConnectBase* usb) {
    if(control && udp && usb) {
      catControl = control;
      ethernetConnection = udp;
      usbConnection = usb;
      enabled = true;
      ethernetConnection->init();
    } else {
      activeConnection = nullptr;
      catControl = nullptr;
      ethernetConnection = nullptr;
      usbConnection = nullptr;
      enabled = false;
    }
  }

  void update() {
    //LinkState state = linkState;

    if(!enabled) return;

    unsigned long now = millis();
    if(now - pollTimer >= POLL_INTERVAL) {
      pollTimer = now;
      switch(linkState) {
        case LINK_DISCONNECTED:     handleDisconnected(); break;
        case LINK_CHECK_CONNECTION: handleConnection();   break;
        case LINK_CHECK_HEARTBEAT:  checkHeartbeat();     break;
        case LINK_CONNECTED:        handleConnected();    break;
        case LINK_LOST:             handleLinkLost();     break;
      }
    }

    //if((role == REMOTE_ROLE_T41) && (state != linkState)) Serial.println(linkState);

    // *** a little pause here is needed sometimes to ensure reconnection ***
    if(linkState != LINK_CONNECTED) delay(1);
  }

  bool isRemote() const { return role == REMOTE_ROLE_REMOTE; }
  bool connected() const { return linkState == LINK_CONNECTED; }
  //Stream* getCommandStream() { return commandStreamPointer; }
  //Stream* getDataStream() { return dataStreamPointer; }

private:
  bool enabled = false;

  // CAT command driver
  CatControl *catControl = nullptr;

  unsigned long pollTimer = 0;
  unsigned long lastHeartbeat = 0;
  unsigned long lastHbTX = 0;
  int heartbeatCount = -1;


  void handleDisconnected() {
    catControl->setStream(nullptr);
    activeConnection = nullptr;
    //activeConnection->end(); // already done by disconnect or not needed

    // check whether a connection has been made
    // an Ethernet connection takes priority since USB connections are sticky
    if(ethernetConnection && ethernetConnection->linkStatus()) {
      activeConnection = ethernetConnection;
    } else if(usbConnection && usbConnection->linkStatus()) {
      activeConnection = usbConnection;
    } else {
      setDisconnected();
      return;
    }

    linkState = LINK_CHECK_CONNECTION;
    t41.RemoteStatus = REMOTE_WAITING;
}

  void handleConnection() {
    if(activeConnection->linkStatus()) {
      if(activeConnection->connected()) {
        catControl->setStream(activeConnection->getCommandStream());
        setConnected();
      } else {
        // a disconnect here overcomes sticky USB connection
        //if(!activeConnection->connect()) setDisconnected();
        activeConnection->connect();
      }
    } else {
      setDisconnected();
    }
  }

  void handleConnected() {
    bool structureHealthy = activeConnection->linkStatus() && activeConnection->connected();

    if(structureHealthy) {
      checkHeartbeat();
    } else {
      setLinkLost();
    }
  }

  void handleLinkLost() {
    // close connections, this is non-blocking
    activeConnection->disconnect();

    // disconnect CAT driver
    catControl->setStream(nullptr);

    setDisconnected();
  }

  void setDisconnected() {
    linkState = LINK_DISCONNECTED;
    t41.RemoteStatus = REMOTE_NOT_CONNECTED;
  }

  void setConnected() {
    linkState = LINK_CHECK_HEARTBEAT;
    checkHeartbeat(true);
  }

  void setLinkLost() {
    linkState = LINK_LOST;
    t41.RemoteStatus = REMOTE_LOST;
  }

  // The check of a physical link is not a reliable indicator of a connection.
  // checkHeartbeat serves that purpose. In the LINK_CONNECTED state,
  // an ID command is sent from the T41 to the remote units every HEARTBEAT_INTERVAL,
  // with catControl->heartbeat recording the time of the reaponse. The remote
  // device considers the receipt of the ID command as a heartbeat.
  // At least HEARTBEAT_COUNT responses must be received before a new connection is
  // considered established and IQ data stream is begun.
  // The connection is considered lost if a heartbeat response is not received within
  // HEARTBEAT_TIMEOUT. In that case, the IQ data stream is stopped and the link lost
  // state is entered.
  void checkHeartbeat(bool reset = false) {
    unsigned long now = millis();
    static unsigned long start = now;

    if(reset) {
      lastHeartbeat = now;
      lastHbTX = now;
      heartbeatCount = 0;  // begin startup heartbeat operation
      start = now;
    } else {
      unsigned long last = lastHeartbeat;
      unsigned long hbInt = HEARTBEAT_INTERVAL;

      lastHeartbeat = catControl->getHeartbeat();

      // heartbeat validation
      if(heartbeatCount >= 0) {
        hbInt = 1000; // send a heartbeat every second during startup

         // startup heartbeat check
        if(lastHeartbeat > last) ++heartbeatCount;

        if(heartbeatCount >= HEARTBEAT_COUNT) {
          // *** probably makes sense for remote to send msg to T41 to start sending data ***
          linkState = LINK_CONNECTED;
          t41.RemoteStatus = REMOTE_CONNECTED;
          SetupRemoteIQStream(activeConnection->getConnectionType());
          heartbeatCount = -1; // begin normal heartbeat operation
        }

        // USB connections are sticky, use heartbeat to disconnect periodically
        // so an Ethernet connection can be detected
        if(now - start > hbInt * (HEARTBEAT_COUNT + 1)) {
          setDisconnected();
          heartbeatCount = -1; // begin normal heartbeat operation
        }
      } else {
         // normal heartbeat check
        if(now - lastHeartbeat > HEARTBEAT_TIMEOUT) {
          Serial.println("Missed heartbeat...");
          setLinkLost();
        }
      }

      // check heartbeat timing
      if(role == REMOTE_ROLE_T41 && (now - lastHbTX >= hbInt)) {
        lastHbTX = now;
        catControl->send("ID;");
      }
    }
  }

};
