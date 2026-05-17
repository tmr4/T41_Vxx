#pragma once

/*
 AudioOutputUDP - Streams 2-channels from connected AudioStream objects to set UDP port

 Works with AudioInputUDP

 *** This object could be made more robust with a buffer overflow checks
     and syncing but early testing hasn't shown a need for this ***

*/

#include <Arduino.h>
#include <AudioStream.h>
#include <QNEthernet.h>
using namespace qindesign::network;

#include "debug.h"

//-------------------------------------------------------------------------------------------------------------
// Forward
//-------------------------------------------------------------------------------------------------------------

void InitEthernet(const IPAddress& ip, const IPAddress& subnet, const IPAddress& gateway);

//-------------------------------------------------------------------------------------------------------------
// Code
//-------------------------------------------------------------------------------------------------------------
