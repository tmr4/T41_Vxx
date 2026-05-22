
#include <Arduino.h>
#include <QNEthernet.h>
#include <USBHost_t36.h>

//using namespace qnetsilverman;
using namespace qindesign::network;

#include "catControl.h"
#include "input_tcp.h"
#include "output_tcp.h"
#include "input_usb.h"
#include "output_usb.h"

//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

enum DeviceRole { ROLE_MAIN, ROLE_REMOTE };
enum ConnectMode { CONNECT_NONE, CONNECT_USB, CONNECT_ETHERNET };
enum LinkState { LINK_DISCONNECTED, LINK_CHECK_HARDWARE, LINK_CONNECTED, LINK_LOST };

class ConnectManager {
private:
  DeviceRole localRole = ROLE_MAIN;
  LinkState linkState = LINK_DISCONNECTED;
  ConnectMode connectMode = CONNECT_NONE;

  const unsigned long POLL_INTERVAL = 40;
  const unsigned long HEARTBEAT_INTERVAL = 500;
  const unsigned long HEARTBEAT_TIMEOUT = 1200;
  const unsigned long TCP_RETRY_INTERVAL = 2000;

  const IPAddress clientIP{192, 168, 1, 101};
  const IPAddress subnet{255, 255, 255, 0};
  const IPAddress gateway{192, 168, 1, 1};
  const IPAddress serverIP{192, 168, 1, 100};

public:
  ConnectManager(uint16_t cPort = 8000, uint16_t dPort = 8001) :
    cmdPort(cPort), dataPort(dPort), tcpCmdServer(cPort), tcpDataServer(dPort),
    usbHostSerial1(usbHost), usbHostSerial2(usbHost) {}

  void begin(DeviceRole role) {
    localRole = role;

    Serial.begin(115200);
    SerialUSB1.begin(115200);

    if(localRole == ROLE_MAIN) {
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
  }

  void update() {
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
  }

  //bool isDataLineActive() const { return linkState == LINK_CONNECTED; }
  //Stream* getCommandStream() { return commandStreamPointer; }
  //Stream* getDataStream() { return dataStreamPointer; }
  //ConnectMode getActiveMode() const { return connectMode; }

private:
  // Remote Control
  CatControl *catControl;

  // Remote Audio - Remote IQ data stream:
  // The T41 IQ data stream is transfered to a remote unit over USB Host/Ethernet. The remote
  // unit receives the data on USB serial/Ethernet. The specific objects are declared below
  // based on the mode selected in the hardware config file, hardwareConfig.h, for each unit.
  //AudioStream audioStream;
  AudioInputSerial1 *usbInput;
  AudioOutputHostSerial *usbOutput;
  AudioInputTCP *tcpInput;
  AudioOutputTCP *tcpOuput;

  unsigned long pollTimer = 0;
  unsigned long lastHeartbeat = 0;
  unsigned long lastHbTX = 0;
  unsigned long tcpRetryTimer = 0;

  uint16_t cmdPort;
  uint16_t dataPort;

  // command sockets
  EthernetServer tcpCmdServer;
  EthernetClient tcpCmdClient;

  // data sockets (governed by command socket state)
  EthernetServer tcpDataServer;
  EthernetClient tcpDataClient;

  // USB Host pipelines
  USBHost usbHost;
  USBSerial_BigBuffer usbHostSerial1; // data
  USBSerial_BigBuffer usbHostSerial2; // command

  bool checkUsbPhysicalLink() {
    // DTR is a reliable indicator that Serial has connected to a host
    // *** it is not a reliable indicator of a disconnect ***
    // *** !Serial is not a reliable indicator of a disconnect ***
    //connected = Serial.dtr();

    //return (localRole == ROLE_MAIN) ? (usbHostSerial1 && usbHostSerial2)
    //        : (Serial && bool(Serial) && SerialUSB1 && bool(SerialUSB1));
    return false; // testing Ethernet connection
  }

  bool checkEthernetPhysicalLink() { return Ethernet.linkState(); }

  void handleDisconnected() {
    catControl->link = nullptr;
    if(localRole == ROLE_MAIN) {
      tcpOuput->client = nullptr;
    } else {
      tcpInput->client = nullptr;
    }

    if(checkUsbPhysicalLink()) {
      connectMode = CONNECT_USB;
      linkState = LINK_CHECK_HARDWARE;
    }
    else if(checkEthernetPhysicalLink()) {
      connectMode = CONNECT_ETHERNET;
      linkState = LINK_CHECK_HARDWARE;
    }
  }

  void handleCheckHardware() {
    if(connectMode == CONNECT_USB && !checkUsbPhysicalLink()) {
      linkState = LINK_DISCONNECTED;
      return;
    }
    if(connectMode == CONNECT_ETHERNET && !checkEthernetPhysicalLink()) {
      linkState = LINK_DISCONNECTED;
      return;
    }

    if(connectMode == CONNECT_USB) {
      //catControl->setLink((localRole == ROLE_MAIN) ? &usbHostSerial2 : &SerialUSB1);
      //tcpOuput->client = (localRole == ROLE_MAIN) ? &usbHostSerial1 : &Serial;
      //setConnected();
    } else if(connectMode == CONNECT_ETHERNET) {
      if(localRole == ROLE_MAIN) {
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
            if(localRole == ROLE_MAIN) {
              tcpOuput->client = &tcpDataClient;
            } else {
              tcpInput->client = &tcpDataClient;
            }
            setConnected();
          }
        }
      } else {
        // Client - CAT command channel controls connection progression
        unsigned long now = millis();

        if(!tcpCmdClient.connected() && !tcpCmdClient.connecting()) {
          if(now - tcpRetryTimer >= TCP_RETRY_INTERVAL) {
            tcpRetryTimer = now;
            //tcpCmdClient.connectNoWait(serverIP, cmdPort);
            tcpCmdClient.abort(); // Clear out old locks
            tcpCmdClient.setConnectionTimeoutEnabled(false);
            tcpCmdClient.connect(serverIP, cmdPort);
            tcpCmdClient.setNoDelay(true);
          }
        }

        // data port connects only after cmd port connects
        if(tcpCmdClient.connected()) {
          if(!tcpDataClient.connected() && !tcpDataClient.connecting()) {
            //tcpDataClient.connectNoWait(serverIP, dataPort);
            tcpDataClient.abort(); // Clear out old locks
            tcpDataClient.setConnectionTimeoutEnabled(false);
            tcpDataClient.connect(serverIP, cmdPort);
            tcpDataClient.setNoDelay(true);
          }

          if(tcpDataClient.connected()) {
            catControl->link = &tcpCmdClient;
            if(localRole == ROLE_MAIN) {
              tcpOuput->client = &tcpDataClient;
            } else {
              tcpInput->client = &tcpDataClient;
            }
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
      if(localRole == ROLE_MAIN) {
        tcpOuput->client = nullptr;
      } else {
        tcpInput->client = nullptr;
      }
    }

    catControl->link = nullptr;
    connectMode = CONNECT_NONE;
    linkState = LINK_DISCONNECTED;
  }

  void setConnected() {
    linkState = LINK_CONNECTED;
    checkHeartbeat(true);
  }

  void setLinkLost() {
    linkState = LINK_LOST;
  }

  void checkHeartbeat(bool reset = false) {
    unsigned long now = millis();

    if(catControl != nullptr) {
      if(reset) {
        lastHeartbeat = now;
        lastHbTX = now;
      } else {
        lastHeartbeat = catControl->heatbeart;
      }

      if(now - lastHbTX >= HEARTBEAT_INTERVAL) {
        lastHbTX = now;
        catControl->send("ID;");
      }
      if(now - lastHeartbeat > HEARTBEAT_TIMEOUT) {
        setLinkLost();
      }
    }
  }

  void initEthernet(const IPAddress& ip) {
    static bool isInitialized = false;

    if(isInitialized) return; // skip if already started

    Ethernet.begin(ip, subnet, gateway);
    isInitialized = true;
  }
};

//-------------------------------------------------------------------------------------------------------------
// Code
//-------------------------------------------------------------------------------------------------------------
