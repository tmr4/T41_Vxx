#pragma once

#include <Arduino.h>
#include <QNEthernet.h>
#include <USBHost_t36.h>

//using namespace qnetsilverman;
using namespace qindesign::network;

#include "AudioConfig.h"
#include "catControl.h"

#include "t41Property.h"

//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

enum DeviceRole { REMOTE_ROLE_T41, REMOTE_ROLE_REMOTE };
enum ConnectMode { CONNECT_NONE, CONNECT_USB, CONNECT_ETHERNET };
enum LinkState { LINK_DISCONNECTED, LINK_CHECK_HARDWARE, LINK_CONNECTED, LINK_LOST };

class ConnectManager {
private:
  DeviceRole role = REMOTE_ROLE_T41;
  LinkState linkState = LINK_DISCONNECTED;
  ConnectMode connectMode = CONNECT_NONE;

  const unsigned long POLL_INTERVAL = 40;
  const unsigned long HEARTBEAT_INTERVAL = 500;
  const unsigned long HEARTBEAT_TIMEOUT = 1200;
  const int HEARTBEAT_COUNT = 3;
  const unsigned long TCP_RETRY_INTERVAL = 2000;

  const IPAddress clientIP{192, 168, 1, 101};
  const IPAddress subnet{255, 255, 255, 0};
  const IPAddress gateway{192, 168, 1, 1};
  const IPAddress serverIP{192, 168, 1, 100};

  void begin() {
    // *** set up usb as appropriate ***
    //usbHostSerial.begin(1000000);
    //usbHostSerial1.begin(1000000);
    //Serial.begin(115200);
    //SerialUSB1.begin(115200);

    // *** TODO: consider multiple calls to begin ***
    if(role == REMOTE_ROLE_T41) {
      initEthernet(serverIP);
      tcpCmdServer.begin(cmdPort);
      tcpDataServer.begin(dataPort);
    } else {
      initEthernet(clientIP);
    }
    tcpCmdClient.setConnectionTimeoutEnabled(false);
    tcpCmdClient.setNoDelay(true);
    tcpDataClient.setConnectionTimeoutEnabled(false);
    tcpDataClient.setNoDelay(true);
    usbHost.begin();
    enabled = true;
  }

public:
  ConnectManager(DeviceRole _role = REMOTE_ROLE_T41, uint16_t cPort = 8000, uint16_t dPort = 8001) :
    role(_role), cmdPort(cPort), dataPort(dPort), tcpCmdServer(cPort), tcpDataServer(dPort),
    usbHostSerial1(usbHost), usbHostSerial2(usbHost, 1) {}

  void begin(CatControl *control, AudioOutputTCP *stream) {
    if(control && stream) {
      catControl = control;
      tcpOuput = stream;
    } else {
      return;
    }
    begin();
  }

  void begin(CatControl *control, AudioInputTCP *stream) {
    if(control && stream) {
      catControl = control;
      tcpInput = stream;
    } else {
      return;
    }
    begin();
  }

  void update() {
    if(!enabled) return;
    usbHost.Task();

    unsigned long now = millis();
    if(now - pollTimer >= POLL_INTERVAL) {
      pollTimer = now;

      switch (linkState) {
        case LINK_DISCONNECTED:   handleDisconnected();   break;
        case LINK_CHECK_HARDWARE: handleCheckHardware();  break;
        case LINK_CONNECTED:      handleConnected();      break;
        case LINK_LOST:           handleLinkLost();       break;
      }
    }

    if(linkState != LINK_CONNECTED) delay(10);
  }

  bool isRemote() const { return role == REMOTE_ROLE_REMOTE; }
  bool connected() const { return linkState == LINK_CONNECTED; }
  //Stream* getCommandStream() { return commandStreamPointer; }
  //Stream* getDataStream() { return dataStreamPointer; }
  //ConnectMode getActiveMode() const { return connectMode; }

private:
  bool enabled = false;

  // Remote Control
  CatControl *catControl = nullptr;

  // Remote Audio - Remote IQ data stream:
  // The T41 IQ data stream is transfered to a remote unit over USB Host/Ethernet. The remote
  // unit receives the data on USB serial/Ethernet. The specific objects are declared below
  // based on the mode selected in the hardware config file, hardwareConfig.h, for each unit.
  //AudioStream audioStream;
  AudioInputSerial1 *usbInput = nullptr;
  AudioOutputHostSerial *usbOutput = nullptr;
  AudioInputTCP *tcpInput = nullptr;
  AudioOutputTCP *tcpOuput = nullptr;

  unsigned long pollTimer = 0;
  unsigned long lastHeartbeat = 0;
  unsigned long lastHbTX = 0;
  int heartbeatCount = -1;
  unsigned long tcpRetryTimer = 0;

  uint16_t cmdPort;
  uint16_t dataPort;

  // command sockets
  EthernetServer tcpCmdServer;
  EthernetClient tcpCmdClient;

  // data sockets (governed by command socket state)
  EthernetServer tcpDataServer;
  EthernetClient tcpDataClient;

  bool isInitialized = false;

  // USB Host pipelines
  USBHost usbHost;
  USBSerial_BigBuffer usbHostSerial1; // data
  USBSerial_BigBuffer usbHostSerial2; // command

  bool checkUsbPhysicalLink() {
    // DTR is a reliable indicator that Serial has connected to a host
    // *** it is not a reliable indicator of a disconnect ***
    // *** !Serial is not a reliable indicator of a disconnect ***
    //connected = Serial.dtr();

    //return (role == REMOTE_ROLE_T41) ? (usbHostSerial1 && usbHostSerial2)
    //        : (Serial && bool(Serial) && SerialUSB1 && bool(SerialUSB1));
    return false; // testing Ethernet connection
  }

  bool checkEthernetPhysicalLink() { return Ethernet.linkState(); }

  void handleDisconnected() {
    catControl->link = nullptr;
    if(role == REMOTE_ROLE_T41) {
      tcpOuput->client = nullptr;
    } else {
      tcpInput->client = nullptr;
    }

    if(checkUsbPhysicalLink()) {
      connectMode = CONNECT_USB;
      linkState = LINK_CHECK_HARDWARE;
      t41.RemoteStatus = REMOTE_WAITING;
    } else if(checkEthernetPhysicalLink()) {
      connectMode = CONNECT_ETHERNET;
      linkState = LINK_CHECK_HARDWARE;
      t41.RemoteStatus = REMOTE_WAITING;
    }
  }

  void handleCheckHardware() {
    if(connectMode == CONNECT_USB && !checkUsbPhysicalLink()) {
      linkState = LINK_DISCONNECTED;
      t41.RemoteStatus = REMOTE_NOT_CONNECTED;
      return;
    }
    if(connectMode == CONNECT_ETHERNET && !checkEthernetPhysicalLink()) {
      linkState = LINK_DISCONNECTED;
      t41.RemoteStatus = REMOTE_NOT_CONNECTED;
      return;
    }

    if(connectMode == CONNECT_USB) {
      //catControl->setLink((role == REMOTE_ROLE_T41) ? &usbHostSerial2 : &SerialUSB1);
      //tcpOuput->client = (role == REMOTE_ROLE_T41) ? &usbHostSerial1 : &Serial;
      //setConnected();
    } else if(connectMode == CONNECT_ETHERNET) {
      if(role == REMOTE_ROLE_T41) {
        // Server - CAT command port controls connection progression
        if(!tcpCmdClient || !tcpCmdClient.connected()) {
          //tcpCmdClient = tcpCmdServer.available();
          tcpCmdClient = tcpCmdServer.accept();
        }

        // data port connects only after cmd port connects
        if(tcpCmdClient && tcpCmdClient.connected()) {
          if(!tcpDataClient || !tcpDataClient.connected()) {
            //tcpDataClient = tcpDataServer.available();
            tcpDataClient = tcpDataServer.accept();
          }

          if(tcpDataClient && tcpDataClient.connected()) {
            catControl->link = &tcpCmdClient;
            tcpOuput->client = &tcpDataClient;
            setConnected();
          }
        }
      } else {
        // Client - CAT command channel controls connection progression
        unsigned long now = millis();

        if(!tcpCmdClient.connected() && !tcpCmdClient.connecting()) {
          if(now - tcpRetryTimer >= TCP_RETRY_INTERVAL) {
            tcpRetryTimer = now;
            tcpCmdClient.abort();
            tcpCmdClient.setConnectionTimeoutEnabled(false);
            tcpCmdClient.connect(serverIP, cmdPort);
            tcpCmdClient.setNoDelay(true);
          }
        }

        // data port connects only after cmd port connects
        if(tcpCmdClient.connected()) {
          if(!tcpDataClient.connected() && !tcpDataClient.connecting()) {
            tcpDataClient.abort();
            tcpDataClient.setConnectionTimeoutEnabled(false);
            tcpDataClient.connect(serverIP, dataPort);
            tcpDataClient.setNoDelay(true);
          }

          if(tcpDataClient.connected()) {
            catControl->link = &tcpCmdClient;
            tcpInput->client = &tcpDataClient;
            setConnected();
          }
        }
      }
    }
  }

  void handleConnected() {
    bool structureHealthy = false;

    if(connectMode == CONNECT_USB) {
      structureHealthy = checkUsbPhysicalLink();
    } else if(connectMode == CONNECT_ETHERNET) {
      // both links must be up
      structureHealthy = (checkEthernetPhysicalLink() &&
                          tcpCmdClient && tcpCmdClient.connected() &&
                          tcpDataClient && tcpDataClient.connected());
    }

    if(!structureHealthy) {
      setLinkLost();
      return;
    }

    checkHeartbeat();
  }

  void handleLinkLost() {
    // close connections, this is non-blocking
    if(connectMode == CONNECT_ETHERNET) {
      tcpCmdClient.stop();
      tcpDataClient.stop();
      if(role == REMOTE_ROLE_T41) {
        tcpOuput->client = nullptr;
      } else {
        tcpInput->client = nullptr;
      }
    }

    catControl->link = nullptr;
    connectMode = CONNECT_NONE;
    linkState = LINK_DISCONNECTED;
    t41.RemoteStatus = REMOTE_NOT_CONNECTED;
  }

  void setConnected() {
    linkState = LINK_CONNECTED;
    t41.RemoteStatus = REMOTE_CONNECTED;
    checkHeartbeat(true);
  }

  void setLinkLost() {
    iqStream.end();
    linkState = LINK_LOST;
    t41.RemoteStatus = REMOTE_LOST;
  }

  // checkUsbPhysicalLink and checkEthernetPhysicalLink are not reliable indicators
  // of connection. checkHeartbeat serves that purpose. In the LINK_CONNECTED state,
  // an ID command is sent from the T41 to the remote units every HEARTBEAT_INTERVAL,
  // with catControl->heatbeart recording the time of the reaponse. The remote
  // device considers the receipt of the ID command as a heartbeat.
  // At least HEARTBEAT_COUNT responses must be received before a new connection is
  // considered established and IQ data stream is begun.
  // The connection is considered lost if a heartbeat response is not received within
  // HEARTBEAT_TIMEOUT. In that case, the IQ data stream is stopped and the link lost
  // state is entered.
  void checkHeartbeat(bool reset = false) {
    unsigned long now = millis();

    if(reset) {
      lastHeartbeat = now;
      lastHbTX = now;
      heartbeatCount = 0;  // begin startup heartbeat operation
    } else {
      unsigned long last = lastHeartbeat;

      lastHeartbeat = catControl->heatbeart;

      // heartbeat validation
      if(heartbeatCount >= 0) {
         // startup heartbeat check
        if(lastHeartbeat > last) ++heartbeatCount;

        if(heartbeatCount >= HEARTBEAT_COUNT) {
          iqStream.begin();
          // *** probably makes sense for remote to send msg to T41 to start sending data ***
          heartbeatCount = -1; // begin normal heartbeat operation
        }
      } else {
         // normal heartbeat check
        if(now - lastHeartbeat > HEARTBEAT_TIMEOUT) {
          setLinkLost();
        }
      }

      // check heartbeat timing
      if(role == REMOTE_ROLE_T41 && (now - lastHbTX >= HEARTBEAT_INTERVAL)) {
        lastHbTX = now;
        catControl->send("ID;");
      }
    }
  }

  void initEthernet(const IPAddress& ip) {
    if(isInitialized) return; // skip if already started

    Ethernet.begin(ip, subnet, gateway);
    isInitialized = true;
  }
};

//-------------------------------------------------------------------------------------------------------------
// Code
//-------------------------------------------------------------------------------------------------------------
