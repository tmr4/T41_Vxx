#pragma once

#include <Arduino.h>
#include <USBHost_t36.h>

#include <QNEthernet.h> // https://github.com/ssilverman/QNEthernet
using namespace qindesign::network;

#include "catControl.h"
#include "t41Property.h"
#include "connectBase.h"
#include "USBManager.h"

/*

Manages the connected state between a T41 and connected remote

Currently supported connections:
  1) Ethernet/USB plug and play CAT control and IQ data

To come:
  2) Ethernet only CAT control and IQ data
  3) USB only CAT control and IQ data
  4) Ethernet only CAT control
  5) USB only CAT control
  6) USB Audio and CAT control (to PC only, requires Serial + MIDI + Audio)

*/

//-------------------------------------------------------------------------------------------------------------
// Forward
//-------------------------------------------------------------------------------------------------------------

//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

enum DeviceRole { DEVICE_ROLE_T41, DEVICE_ROLE_REMOTE };
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
  ConnectManager(DeviceRole _role = DEVICE_ROLE_T41) : role(_role) {}

private:
  DeviceRole role = DEVICE_ROLE_T41;
  LinkState linkState = LINK_DISCONNECTED;

  const unsigned long POLL_INTERVAL = 40;
  //const unsigned long HEARTBEAT_INTERVAL = 15000;
  const unsigned long HEARTBEAT_INTERVAL = 2000;
  //const unsigned long HEARTBEAT_INTERVAL = 1000;
  //const unsigned long HEARTBEAT_INTERVAL = 500;
  const unsigned long HEARTBEAT_TIMEOUT = (HEARTBEAT_INTERVAL * 3);
  const int HEARTBEAT_COUNT = 2;

public:

  void begin(CatControl* cat, ConnectBase* ethernet, ConnectBase* usb) {
    if(cat) {
      catControl = cat;
    } else {
      catControl = nullptr;
    }
    if(ethernet) {
      ethernetConnection = ethernet;
      ethernetConnection->init();
    } else {
      ethernetConnection = nullptr;
    }
    if(usb) {
      usbConnection = usb;
      usbConnection->init();
    } else {
      usbConnection = nullptr;
    }
    if(cat && ethernet) {
      enabled = true;
    } else {
      activeConnection = nullptr;
      enabled = false;
    }
  }

  void update() {
    //LinkState state = linkState;

    if(!enabled) return;

    //if((role == DEVICE_ROLE_T41) && (activeConnection == usbConnection)) USBManager::getHost().Task();
    if(role == DEVICE_ROLE_T41) USBManager::getHost().Task();

    // don't wait for POLL_INTERVAL if link state is LINK_CHECK_HEARTBEAT
    // startup checkHeartbeat has it's own timer
    if(linkState == LINK_CHECK_HEARTBEAT) checkHeartbeat();

    unsigned long now = millis();
    if(now - pollTimer >= POLL_INTERVAL) {
      pollTimer = now;
      switch(linkState) {
        case LINK_DISCONNECTED:     handleDisconnected(); break;
        case LINK_CHECK_CONNECTION: handleConnection();   break;
        case LINK_CHECK_HEARTBEAT:  break;
        case LINK_CONNECTED:        handleConnected();    break;
        case LINK_LOST:             handleLinkLost();     break;
      }
    }

    //if((role == DEVICE_ROLE_T41) && (state != linkState)) Serial.println(linkState);

    // *** a little pause here is needed sometimes to ensure reconnection ***
    if(linkState != LINK_CONNECTED) delay(1);
  }

  ConnectBase* getActiveConnection() { return activeConnection; }

  bool isRemote() const { return role == DEVICE_ROLE_REMOTE; }
  bool connected() const { return linkState == LINK_CONNECTED; }

private:
  bool enabled = false;

  // CAT command driver
  CatControl *catControl = nullptr;

  unsigned long pollTimer = 0;
  unsigned long lastHeartbeat = 0;
  unsigned long lastHbTX = 0;
  int heartbeatCount = -1;

  void yield();

  void handleDisconnected() {
    if(activeConnection) activeConnection->end(); // already done by disconnect or not needed
    catControl->end(); // prevent spurious CAT commands stream
    activeConnection = nullptr;

    // check whether a connection has been made
    // an Ethernet connection takes priority since USB connections are sticky
    if(ethernetConnection && ethernetConnection->linkStatus()) {
      activeConnection = ethernetConnection;
    } else if(usbConnection && usbConnection->linkStatus()) {
      activeConnection = usbConnection;
    } else {
      return;
    }

    linkState = LINK_CHECK_CONNECTION;
    t41.RemoteStatus = REMOTE_WAITING;
  }

  void handleConnection() {
    if(activeConnection && activeConnection->linkStatus()) {
      if(activeConnection->connected()) {
        catControl->setConnectBase(activeConnection, activeConnection == usbConnection);
        setConnected();
      } else {
        // a disconnect here overcomes sticky USB connection
        //if(!activeConnection->connect()) setDisconnected();
        activeConnection->connect();
      }
    } else {
      //setDisconnected();
      setLinkLost();
    }
  }

  void handleConnected() {
    bool structureHealthy = false;

    if(activeConnection) {
      structureHealthy = activeConnection->linkStatus() && activeConnection->connected();
    }

    if(structureHealthy) {
      checkHeartbeat();
    } else {
      setLinkLost();
    }
  }

  void handleLinkLost() {
    // close connections, this is non-blocking
    if(activeConnection) activeConnection->disconnect();

    // disconnect CAT driver
    catControl->end(); // prevent spurious CAT commands stream

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
    //Serial.println("Link lost...");

    catControl->end(); // prevent spurious CAT commands stream
    linkState = LINK_LOST;
    t41.RemoteStatus = REMOTE_LOST;
  }

  void checkHeartbeat(bool reset = false);
};
