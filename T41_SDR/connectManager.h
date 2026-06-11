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
enum ConnectMode { CONNECT_NONE, CONNECT_USB, CONNECT_ETHERNET };
enum LinkState { LINK_DISCONNECTED, LINK_CHECK_HARDWARE, LINK_HEARTBEAT, LINK_CONNECTED, LINK_LOST };

class ConnectManager {
private:
  DeviceRole role = REMOTE_ROLE_T41;
  LinkState linkState = LINK_DISCONNECTED;
  ConnectMode connectMode = CONNECT_NONE;

  const unsigned long POLL_INTERVAL = 40;
  //const unsigned long HEARTBEAT_INTERVAL = 15000;
  const unsigned long HEARTBEAT_INTERVAL = 2000;
  //const unsigned long HEARTBEAT_INTERVAL = 1000;
  //const unsigned long HEARTBEAT_INTERVAL = 500;
  const unsigned long HEARTBEAT_TIMEOUT = (HEARTBEAT_INTERVAL * 3);
  const int HEARTBEAT_COUNT = 2;
  const unsigned long TCP_RETRY_INTERVAL = 2000;

  const IPAddress clientIP{192, 168, 1, 101};
  const IPAddress subnet{255, 255, 255, 0};
  const IPAddress gateway{192, 168, 1, 1};
  const IPAddress serverIP{192, 168, 1, 100};

  void begin() {
    #if RADIO_ROLE == 1
    usbSerialCmd.begin(115200);
    usbSerialData.begin(115200);
    usbOutput->init(&USBManager::getHost(), &usbSerialData);
    #elif RADIO_ROLE == 2
      //Serial.begin(115200);
      SerialUSB1.begin(115200);
    #endif

    // *** TODO: consider multiple calls to begin ***
    if(role == REMOTE_ROLE_T41) {
      initEthernet(serverIP);
      tcpCmdServer.begin(cmdPort);
    } else {
      initEthernet(clientIP);
    }
    tcpCmdClient.setConnectionTimeoutEnabled(false);
    tcpCmdClient.setNoDelay(true);
    enabled = true;
  }

public:
  ConnectManager(DeviceRole _role = REMOTE_ROLE_T41, uint16_t cPort = 8000, uint16_t dPort = 8001) :
    role(_role), cmdPort(cPort), dataPort(dPort), tcpCmdServer(cPort),
    usbSerialCmd(USBManager::getHost(), 1), usbSerialData(USBManager::getHost()) {}

  void begin(CatControl *control, AudioOutputUDP* udp, AudioOutputHostSerial* usb) {
    if(control && udp && usb) {
      catControl = control;
      udpOutput = udp;
      usbOutput = usb;
    } else {
      return;
    }
    begin();
  }

  void begin(CatControl *control, AudioInputUDP* udp, AudioInputSerial1* usb) {
    if(control && udp && usb) {
      catControl = control;
      udpInput = udp;
      usbInput = usb;
    } else {
      return;
    }
    begin();
  }

  void update() {
    //LinkState state = linkState;

    if(!enabled) return;

    //if(connectMode == CONNECT_USB) USBManager::getHost().Task();
    //USBManager::getHost().Task();
    if(role == REMOTE_ROLE_T41) USBManager::getHost().Task();

    unsigned long now = millis();
    if(now - pollTimer >= POLL_INTERVAL) {
      pollTimer = now;
      switch(linkState) {
        case LINK_DISCONNECTED:   handleDisconnected();   break;
        case LINK_CHECK_HARDWARE: handleCheckHardware();  break;
        case LINK_HEARTBEAT:      checkHeartbeat();       break;
        case LINK_CONNECTED:      handleConnected();      break;
        case LINK_LOST:           handleLinkLost();       break;
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
  AudioInputUDP *udpInput = nullptr;
  AudioOutputUDP *udpOutput = nullptr;

  unsigned long pollTimer = 0;
  unsigned long lastHeartbeat = 0;
  unsigned long lastHbTX = 0;
  int heartbeatCount = -1;
  unsigned long tcpRetryTimer = 0;

  static constexpr uint16_t udpBuffer = (RADIO_ROLE == 1) ? 1 : 32;

  uint16_t cmdPort;
  uint16_t dataPort;

  // command sockets
  EthernetServer tcpCmdServer;
  EthernetClient tcpCmdClient;

  // data sockets (governed by command socket state)
  EthernetServer tcpDataServer;
  EthernetUDP udpDataClient{udpBuffer};

  bool isInitialized = false;

  // USB Host pipelines
  USBSerial_BigBuffer usbSerialCmd;  // command
  USBSerial_BigBuffer usbSerialData; // data

  bool checkUsbPhysicalLink() {
    // DTR is a reliable indicator that Serial has connected to a host
    // *** it is not a reliable indicator of a disconnect ***
    // *** !Serial is not a reliable indicator of a disconnect ***

    // *** TODO: look for better USB host connection indicator ***

    //return (role == REMOTE_ROLE_T41) ? (usbSerialData && usbSerialCmd)
    //        : (Serial && bool(Serial) && SerialUSB1 && bool(SerialUSB1));
    //return false; // testing Ethernet connection
    #if RADIO_ROLE == 1
    //return false; // testing Ethernet connection
    USBManager::getHost().Task();
    return usbSerialData && usbSerialCmd;
    #elif RADIO_ROLE == 2
    return Serial.dtr();
    #endif
  }

  bool checkEthernetPhysicalLink() { return Ethernet.linkState(); }

  void handleDisconnected() {
    catControl->setLink(nullptr);

    if(connectMode == CONNECT_USB) {
      if(role == REMOTE_ROLE_T41) {
        usbOutput->end();
        //usbSerialCmd.end();
        //usbSerialData.end();
      } else {
        usbInput->end();
      }
    } else if(connectMode == CONNECT_ETHERNET) {
      if(role == REMOTE_ROLE_T41) {
        udpOutput->end();
        udpOutput->setClient(nullptr);
      } else {
        udpInput->setClient(nullptr);
        udpInput->end();
      }
    }

    // an Ethernet connection takes priority since USB connections are sticky
    if(checkEthernetPhysicalLink()) {
      connectMode = CONNECT_ETHERNET;
      linkState = LINK_CHECK_HARDWARE;
      t41.RemoteStatus = REMOTE_WAITING;
    }else if(checkUsbPhysicalLink()) {
      connectMode = CONNECT_USB;
      linkState = LINK_CHECK_HARDWARE;
      t41.RemoteStatus = REMOTE_WAITING;
    } else {
      setDisconnected();
    }
  }

  void handleCheckHardware() {
    // an Ethernet connection takes priority since USB connections are sticky
    if(connectMode == CONNECT_ETHERNET) {
      if(checkEthernetPhysicalLink()) {
        if(role == REMOTE_ROLE_T41) {
          // Server - CAT command port controls connection progression
          if(!tcpCmdClient || !tcpCmdClient.connected()) {
            tcpCmdClient = tcpCmdServer.accept();
          }

          // data port starts only after cmd port connects
          if(tcpCmdClient && tcpCmdClient.connected()) {
            catControl->setLink(&tcpCmdClient);
            udpOutput->setClient(&udpDataClient, clientIP, dataPort);
            udpDataClient.begin(dataPort);
            setConnected();
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

          // data port starts only after cmd port connects
          if(tcpCmdClient.connected()) {
            catControl->setLink(&tcpCmdClient);
            udpInput->setClient(&udpDataClient);
            udpDataClient.begin(dataPort);
            setConnected();
          }
        }
      } else {
        setDisconnected();
        return;
      }
    }

    //if(connectMode == CONNECT_USB && !checkUsbPhysicalLink()) {
    if(connectMode == CONNECT_USB) {
      if(checkUsbPhysicalLink()) {
        #if RADIO_ROLE == 1
        catControl->setLink(&usbSerialCmd);
        //usbSerialCmd.begin(115200);
        //usbSerialData.begin(115200);
        #elif RADIO_ROLE == 2
        catControl->setLink(&Serial);
        #endif
        setConnected();
      } else {
        setDisconnected();
        return;
      }
    }
  }

  void handleConnected() {
    bool structureHealthy = false;

    if(connectMode == CONNECT_ETHERNET) {
      structureHealthy = (checkEthernetPhysicalLink() && tcpCmdClient && tcpCmdClient.connected());
    } else if(connectMode == CONNECT_USB) {
      structureHealthy = checkUsbPhysicalLink();
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
      if(Ethernet.linkState()) {
        // just stop if link is still up
        tcpCmdClient.stop();
      } else {
        // abort on cable loss
        tcpCmdClient.abort();
      }
      if(role == REMOTE_ROLE_T41) {
        udpOutput->setClient(nullptr);
      } else {
        udpInput->setClient(nullptr);
      }
    }

    catControl->setLink(nullptr);
    setDisconnected();
}

  void setDisconnected() {
    connectMode = CONNECT_NONE;
    linkState = LINK_DISCONNECTED;
    t41.RemoteStatus = REMOTE_NOT_CONNECTED;
  }

  void setConnected() {
    linkState = LINK_HEARTBEAT;
    checkHeartbeat(true);
  }

  void setLinkLost() {
    iqStream->end();
    linkState = LINK_LOST;
    t41.RemoteStatus = REMOTE_LOST;
  }

  // checkUsbPhysicalLink and checkEthernetPhysicalLink are not reliable indicators
  // of connection. checkHeartbeat serves that purpose. In the LINK_CONNECTED state,
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
          SetupRemoteIQStream(connectMode);
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

  void initEthernet(const IPAddress& ip) {
    if(isInitialized) return; // skip if already started

    Ethernet.begin(ip, subnet, gateway);
    isInitialized = true;
  }
};

//-------------------------------------------------------------------------------------------------------------
// Code
//-------------------------------------------------------------------------------------------------------------
