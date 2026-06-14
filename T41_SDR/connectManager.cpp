
#include "SDT.h"

#include "connectManager.h"

//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

//-------------------------------------------------------------------------------------------------------------
// Forward
//-------------------------------------------------------------------------------------------------------------

void SetupRemoteIQStream(ConnectMode connectMode);
void YieldToProcess(bool updateSpectrum = false);

//-------------------------------------------------------------------------------------------------------------
// Code
//-------------------------------------------------------------------------------------------------------------

void ConnectManager::yield() {
  YieldToProcess();
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
void ConnectManager::checkHeartbeat(bool reset /* = false */) {
  unsigned long now = millis();
  static unsigned long start = now;

  if(reset) {
    lastHeartbeat = now;
    lastHbTX = 0;
    heartbeatCount = 0;  // begin startup heartbeat operation
    start = now;
  } else {
    unsigned long last = lastHeartbeat;
    unsigned long hbInt = HEARTBEAT_INTERVAL;

    lastHeartbeat = catControl->getHeartbeat();

    // heartbeat validation
    if(heartbeatCount >= 0) {
      //hbInt = 1000; // send a heartbeat every second during startup
      hbInt = 500; // send a heartbeat every half-second during startup

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
      if(now - start > HEARTBEAT_TIMEOUT) {
        //Serial.println("Startup heartbeat failure...");
        setLinkLost();
        heartbeatCount = -1; // begin normal heartbeat operation
        return;
      }
    } else {
        // normal heartbeat check
      if(now - lastHeartbeat > HEARTBEAT_TIMEOUT) {
        //Serial.println("Missed heartbeat...");
        setLinkLost();
        return;
      }
    }

    // check heartbeat timing
    if(role == DEVICE_ROLE_T41 && (now - lastHbTX >= hbInt)) {
      lastHbTX = now;
      catControl->send("ID;");
    }
  }
}
