#pragma once

//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

class Telemetry {
public:
  Telemetry() {}

  void init(size_t _head, size_t _tail, int available) {
    inEvent = true;
    startTime = millis();

    lowAvailableCount = 0;

    eventDuration = 0;
    maxLaspe = 0;
    lastInLoopCheck = 0;

    totalClearEvents = 0;
    consecutiveClears = 0;
    recentClears = 0;

    latestAvailable = 0;
    //availableAtStart = latestAvailable;
    availableAtStart = available;
    head = _head;
    tail = _tail;
    audioMemoryUsageMax = AudioMemoryUsageMax();
    audioMemoryUsage = AudioMemoryUsage();
  }

  void bufferClearEvent(size_t head, size_t tail, int available) {
    //if(tail != head) Serial.printf("clearing output queue: head: %u tail: %u\n", head, tail);
    if(!inEvent) init(head, tail, available);

    ++totalClearEvents;
    ++consecutiveClears;
    ++recentClears;
  }

  void preLoopCheck(size_t head, size_t tail, int available) {
    if(inEvent) {
      latestAvailable = available;
      if(latestAvailable < 512) {
        ++lowAvailableCount;
      }
    }
  }

  void inLoopCheck(size_t head, size_t tail, int available) {
    if(inEvent) {
      unsigned long laspe = millis() - lastInLoopCheck;

      if(laspe > maxLaspe) maxLaspe = laspe;
      lastInLoopCheck = millis();

      if(recentClears == 0) {
        eventDuration = millis() - startTime;

        // Report and Reset
        logEventReport();
        inEvent = false;
      }
      recentClears = 0;
    }
  }

private:
  bool inEvent = false;
  unsigned long startTime = 0;

  size_t head = 0;
	size_t tail = 0;

  size_t latestAvailable = 0;
  uint32_t lowAvailableCount = 0;
  unsigned long eventDuration = 0;

  uint32_t totalClearEvents = 0;
  uint32_t consecutiveClears = 0;
  uint32_t recentClears = 0;

  size_t availableAtStart = 0;

  uint32_t lastInLoopCheck = 0;
  uint32_t maxLaspe = 0;

  uint32_t audioMemoryUsageMax = 0;
  uint32_t audioMemoryUsage = 0;

  void logEventReport() {
    Serial.println(F("\n--- Network Event Report ---"));

    Serial.printf(F("Event duration: %u ms\n"), eventDuration);

    Serial.printf(F("Clear events: %u\n"), consecutiveClears);

    if(consecutiveClears > 0) {
      Serial.printf(F("Avg time between clears: %u ms\n"), eventDuration / consecutiveClears);
    }

    Serial.printf(F("TCP blocked count: %u\n"), lowAvailableCount);

    Serial.printf(F("TCP buffer at start: %u\n"), availableAtStart);

    Serial.printf(F("Queue status at start: head: %u tail: %u\n"), head, tail);
    Serial.printf(F("Audio memory usage at start: current: %u max: %u\n"), audioMemoryUsage, audioMemoryUsageMax);

    Serial.println(F("Status: resuming normal ops"));
    Serial.println(F("---------------------------\n"));
  }

};

extern Telemetry telemetry;
