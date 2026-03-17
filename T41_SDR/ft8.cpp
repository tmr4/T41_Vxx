
// T41 FT8 specifics:
// External FT8
//  - switches to 44.1k sample rate
//  - returns to 192k sample rate
//
// Internal FT8 processing (with modified ft8_lib from: https://github.com/kgoba/ft8_lib)
//  - decoding over the air and wav file FT8
//  - encoding set message (to come)

// ft8 synch modified from: https://github.com/DD4WH/Pocket_FT8

#include <math.h>
#include <stdint.h>
#include <time.h>
#include <TimeLib.h>                   // Part of Teensy Time library

#include "SDT.h"

#include "AudioConfig.h"
#include "ButtonProc.h"
#include "Display.h"
#include "ft8.h"
#include "InfoBox.h"
#include "Utility.h"

#include "debug.h"

//#include "..\src\hardware.h"
#include "src\hardware.h"
#include "src\hardwareConfig.h"

//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

#define DEBUG_MSG(msg)
//#define DEBUG_MSG(msg) Serial.println(msg)

#define DEBUG_LOC(msg)
//#define DEBUG_LOC(msg) Serial.println(msg)

// RX/TX debug messages overwhelm output, treat them separately
#define DEBUG_RXTX(msg)
//#define DEBUG_RXTX(msg) Serial.println(msg)

#define DEBUG_MEM(msg)
//#define DEBUG_MEM(msg) Serial.print(msg); Serial.print(": "); Serial.println(AudioMemoryUsageMax()); AudioMemoryUsageMaxReset();


#ifdef USE_BUFFERED_FT8_WAV
EXTMEM float32_t ft8WavBuf[15 * 12000]; // buffer a FT8 wav file for use with decoder
//int numWavBuf = 0, countWavBuf = 1920 * 14, countWavBufStart = 1920 * 14; // first 14 frames are zero, gives -0.8 sec offset vs +1.3 sec for original wave file
//int numWavBuf = 0, countWavBuf = 1920 * 7, countWavBufStart = 1920 * 7; // first 14 frames are zero, gives +0.2 sec offset
int numWavBuf = 0, countWavBuf = 0, countWavBufStart = 0;
#endif

float *ft8TxSignalBuf = NULL;

#define RA8875_GREEN 0x07E0 // 0, 255, 0

char baseCall[14], baseGrid[5];

typedef struct {
  char msg[35]; // FTX_MAX_MESSAGE_LENGTH = callsign[13] + space + callsign[13] + space + report[6] + terminator

  // three parts of FT8 message
  // *** put together with: sprintf(message,"%.13s %.13s %.6s",field1, field2, field3); ***
  char field1[20];
  char field2[20];
  char field3[20];

  float  freq; // hz

  uint8_t hour, min, sec;
  float time_sec;
  tm slot_time;

  int  sync_score;
  float  snr;
  //int  distance; // *** needs SetStationCoordinates in init ***
} RxMsg;

// TX message status
#define MSG_WAITING     0
#define MSG_NEXT        1
#define MSG_SENT        2
#define MSG_ACK         3
#define MSG_TIMEOUT     4
#define MSG_COMPLETED   5

typedef struct {
  char msg[35]; // FTX_MAX_MESSAGE_LENGTH = callsign[13] + space + callsign[13] + space + report[6] + terminator

  // three parts of FT8 message
  // *** put together with: sprintf(message,"%.13s %.13s %.6s",field1, field2, field3); ***
  char field1[20];
  char field2[20];
  char field3[20];

  float  freq; // hz

  //uint8_t hour, min, sec;
  //float time_sec;
  tm slot_time;

  float  snr;

  // 0: waiting
  // 1: next to TX
  // 2: sent
  // 3: reply received/acknowledged
  // 4: no response after 10 tries (transmission disabled)
  // 5: QSO completed
  int status;

  int tries;
} TxMsg;

typedef struct {
  int type; // 0: CQ, 1: CQ reply

  int tx[3], rx[3];

  // 0: waiting
  // 1: completed
  // 2: abandoned
  int status;
} QsoView;

/*

Detailed Message List:
The rxBuf list records the details of each decoded message.  It is sized to capture all
messages from an FT8 session.

FT8 Message Window:
The FT8 message window has three subwindows for (1) all messages, left, (2) CQ messages, middle,
and (3) messages around the RX frequency, right.  The subwindows are an FT8_MSG_ROWS row snapshot into the
associated message lists with the most recent messages toward the bottom of the list.  The windows are
scrollable with the index of the message at the top of the window as a global variable.

Subwindow Message Lists:
There is an integer list associated with each subwindow.  The integer is an index into the rxBuf
message list which is longer and . The lists are a circular
buffer with the most recent messages at and before the head of the list and the oldest messages immediately after the head.

This structure follows a few rules:
  (1) If the window top is the list head then only a single message is shown.
  (2) The message window can never straddle the list head.
  (3) If a new message is added to a list in the location of the window top then window top is incremented.
  (4)

*/

//#define MAX_DECODED_MESSAGES 10 // for testing of decode list getting full
//#define MAX_DECODED_MESSAGES 500
#define MAX_DECODED_MESSAGES 2400 // 10 msgs/interval * 4 intervals/min * 60 min/hr

// Message buffers
// store details of RX and TX messages
EXTMEM RxMsg rxBuf[MAX_DECODED_MESSAGES];
EXTMEM TxMsg txBuf[500]; // *** TODO: refine size ***

// msg window list size is a tradeoff of scrolling vs having old msg overwritten before desired
// having the allList separate from the details list allows a filter on the list (*** TODO: impliment ***)
//#define MAX_LIST_MESSAGES 50
#define MAX_LIST_MESSAGES 500

// Message lists
// store index into message buffers for various views
EXTMEM int allList[MAX_LIST_MESSAGES], rxList[MAX_LIST_MESSAGES], cqList[MAX_LIST_MESSAGES];

EXTMEM QsoView qsoList[MAX_LIST_MESSAGES];

bool qsoViewActive = false;

// Message windows
#define ALL_WINDOW 0
#define CQ_WINDOW 1
#define RX_WINDOW 2

int decodedMsgs = 0, txMsgs = 0;
int allMsgs, cqMsgs = 0, rxMsgs = 0; // number of msgs in each list
int allHead = -1, cqHead = -1, rxHead = -1; // head gets incremented prior to msg added to list
bool allScroll = false, cqScroll = false, rxScroll = false; // false: latest msgs shown, true: msg list scrollable
int allTop, cqTop, rxTop; // message window list top message index

int activeMsg = 0; // selected msg

uint32_t current_time, start_time, ft8_time;

// *** TODO: reconsider use of fixed row height vs display dependent in routines ***
#define FT8_MSG_ROWS    10
#define FT8_ROWS        13
#define FT8_ROW_HEIGHT  16
#define FT8_COL_WIDTH   8

// selected msg detail and tx queue below msg lists
//#define FT8_WINDOW_TOP          (YPIXELS - FT8_ROW_HEIGHT * FT8_ROWS)
//#define FT8_MSG_LIST_TOP        (YPIXELS - FT8_ROW_HEIGHT * (FT8_MSG_ROWS + 2))
//#define FT8_MSG_LIST_SUMMARY    (YPIXELS - FT8_ROW_HEIGHT * FT8_ROWS)
//#define FT8_MSG_WINDOW_DETAIL   (YPIXELS - FT8_ROW_HEIGHT * 2)
//#define FT8_TX_QUEUE_TOP        (YPIXELS - FT8_ROW_HEIGHT)

// selected msg detail above and tx queue below msg lists
#define FT8_WINDOW_TOP          (YPIXELS - FT8_ROW_HEIGHT * FT8_ROWS)
#define FT8_MSG_LIST_TOP        (YPIXELS - FT8_ROW_HEIGHT * (FT8_MSG_ROWS + 1))
#define FT8_MSG_LIST_SUMMARY    (YPIXELS - FT8_ROW_HEIGHT * (FT8_MSG_ROWS + 2))
#define FT8_TX_QUEUE_TOP        (YPIXELS - FT8_ROW_HEIGHT)
#define FT8_MSG_WINDOW_DETAIL   FT8_WINDOW_TOP

// FT8 processing
int bufCount = 0;
int frameCount = 0;

//bool ft8SpectrumFlag; // true when ft8 frequency spectrum data is ready to be drawn
bool ft8WavFlag;      // true when ft8 wav file has closed (signals need to shift to DEMOD_FT8_INTERNAL mode)

// FT8 Decoder States
#define STATE_BUFFERING   0
#define STATE_PROCESSING  1
#define STATE_DECODING    2
#define STATE_RX_UPDATE   3
#define STATE_TX          4
#define STATE_TX_UPDATE   5

int ft8DecoderState = STATE_BUFFERING;

// FT8 Decoder Option States
// *** these are also status for info box ***
int ft8SyncState = 0; // sync status: 0 - not sync'd, 1 - sync'd
int ft8TxState = 0; // auto transmission: 0 - off, 1 - enabled (transmission will start automatically at next tx interval)
int ft8IntState = 0; // Tx interval: 0 - even, 1 - odd
int ft8CqState = 0; // CQ response: 0 - manual, 1 - auto (respond automatically to strongest signal in last RX cycle)

int master_offset, offset_step;

bool ft8Init = false;

int ft8TxFreq = 1000;
int ft8RxFreq = 1000;
bool txEqualsRx = true;

// internal transmission queue
// Follows message numbering structure of FT8 whitepaper section 7.
// https://wsjt.sourceforge.io/FT4_FT8_QEX.pdf, except TX6 is txQueue[0]
// example from whitepaper
// index 0,2,4 are calling CQ QSOs
//txQueue[0] = "CQ K1JT FN20";  // expected RX "K1JT K9AN EN50"
//txQueue[2] = "K9AN K1JT +05"; // expected RX "K1JT K9AN R-12"
//txQueue[4] = "K9AN K1JT RRR"; // expected RX "K1JT K9AN 73"

// index 1,3,5 are for replying to CQ
//txQueue[1] = "K1JT K9AN EN50" // expected RX to "CQ K1JT FN20";
//txQueue[3] = "K1JT K9AN R-12  // expected RX to "K9AN K1JT +05";
//txQueue[5] = "K1JT K9AN 73"   // expected RX to "K9AN K1JT RRR";
TxMsg *txQueue[6] = {NULL}; // pointers into txBuf for appropriate msgs

// FT8 TX next outgoing message
#define QSO_MSG_0     0
// we've had a reply to our CQ
#define QSO_MSG_2     2
#define QSO_MSG_4     4
// we've replied to a CQ
#define QSO_MSG_1     1
#define QSO_MSG_3     3
#define QSO_MSG_5     5

// index of next message in txQueue to be transmitted
int txNextMsg = QSO_MSG_0; // default msg

extern bool ft8PTT;

//-------------------------------------------------------------------------------------------------------------
// Forwards
//-------------------------------------------------------------------------------------------------------------

int SetI2SFreq(int freq);
void InitFFTArrays();
void InitHilbertFilters();
void SetupDemodFilterBW();
void ResetTuning();

void YieldToProcess(bool updateSpectrum = false);
void YieldForProcess(int ms);

void AddDecodedMessage(struct tm *tmSlot, int16_t score, float time_sec, float freq, char *msg);

void PrepareFT8ExciterIQData(float *sig);

void ChangeFt8TxState(int wheel);

// ft8lib
bool ft8lib_InitDecoder();
void ft8lib_ExitDecoder();

bool ft8lib_BufferSignal(float *buf, int sizeBuf, int offset);
bool ft8lib_ProcessFrame(int frame);
void ft8lib_Decode(struct tm *start);
uint8_t *ft8lib_GetFT8SpectrumData(int symbol);

bool ft8lib_GenFT8(char *message, float frequency);
float *ft8lib_GetSignal();

//-------------------------------------------------------------------------------------------------------------
// External FT8 Code
//-------------------------------------------------------------------------------------------------------------

FLASHMEM bool InitFT8() {
  bool result = true;

  if(sampleRate > 50000) {
    sampleRate = 44100.0;
    intermediateFreq = 11025.0;
    // using 48k sample rate doesn't change FT8 transmision
    //sampleRate = 48000.0;
    //intermediateFreq = 12000.0;
    SetI2SFreq(sampleRate);
    InitFFTArrays();
    SetZoom(1);
    //InitZoomFFTFilter(); // *** TODO: can save some memory by specifying block size if will operate in FT8 a lot ***
    InitHilbertFilters();
    SetupDemodFilterBW();
    ResetTuning();
  }

  return result;
}

FLASHMEM void ExitFT8() {
  if(sampleRate < 50000) {
    sampleRate = 192000.0;
    intermediateFreq = 48000.0;
    SetI2SFreq(sampleRate);
    InitFFTArrays();
    SetZoom(1);
    InitHilbertFilters();
    SetupDemodFilterBW();
  }
}

//-------------------------------------------------------------------------------------------------------------
// Internal FT8 Testing Code
//-------------------------------------------------------------------------------------------------------------

//#define SPECTRUM_TESTING // plots frequency spectrum frame instead of waterfall (change frame with volume knob)
#define TX_TESTING // generates mock RX messages for "CQ KN6ZDE CM87" TX test

#ifdef SPECTRUM_TESTING
EXTMEM uint8_t freqSpectrum[512*79];

FLASHMEM void DrawTestSpectrum(uint8_t *spec) {
  int yPlot, y1Plot = 0;

  // clear old spectrum
  //EraseSpectrumWindow();
  tft.fillRect(WATERFALL_L, FT8_WINDOW_TOP, WATERFALL_W, FT8_ROW_HEIGHT * FT8_ROWS + 3, RA8875_BLACK);

  for(int i = 0; i < 512; i++) {
    // plot within spectrum area
    //yPlot = SPECTRUM_BOTTOM - spec[i] / 2;
    //y1Plot = SPECTRUM_BOTTOM - spec[i + 1] / 2;
    //tft.drawLine(SPECTRUM_LEFT_X + i, y1Plot, SPECTRUM_LEFT_X + i, yPlot, RA8875_YELLOW);

    // plot within message area
    yPlot = YPIXELS - 10 - (int)((float)spec[i] / 1.5);
    y1Plot = YPIXELS - 10 - (int)((float)spec[i + 1] / 1.5);
    tft.drawLine(WATERFALL_L + i, y1Plot, WATERFALL_L + i, yPlot, RA8875_YELLOW);
  }
}
#endif

// TX_TESTING only available on project system to prevent inadvertent transmissions
// *** this won't compile on other systems ***
// *** TODO: consider relaxing this w/ mock transmission mode on other versions ***
#if defined(TX_TESTING) && defined(PROJECTSYSTEM)

void DisplayMessages(int window, int *list, int numMsgs, bool scroll, int &top, int head, int max);
void DisplayAllMessages();
void DisplayTxMessages();
TxMsg *AddTxMsg(char *msg, float freq, float  snr);

void AddRxResponse(char *msg, float freq, struct tm *tmSlot, int16_t score) {
  float time_sec = 0.0;

  AddDecodedMessage(tmSlot, score, time_sec, freq, msg);
  //DisplayMessages(RX_WINDOW, rxList, rxMsgs, rxScroll, rxTop, rxHead, MAX_LIST_MESSAGES);
  DisplayAllMessages();
}

bool MockMsgTraffic() {
  TxMsg *txMsg = txQueue[txNextMsg];
  struct tm tmSlot = { .tm_sec = (second() / 15) * 15, .tm_min = minute(), .tm_hour = hour() };
  bool result = true;

  // mock replies to my CQ
  char msg1[35] = "KN6ZDE K9AN EN50"; // mock RX to "CQ KN6ZDE CM87"
  char msg3[35] = "KN6ZDE K9AN +05";  // mock RX to "KN6ZDE K9AN R-12"
  char msg5[35] = "KN6ZDE K9AN RRR";  // mock RX to "KN6ZDE K9AN 73"

  //char msg0[35] = "CQ KN6ZDE CM87";
  char msg2[35] = "KN6ZDE K9AN R-12"; // expected RX "K1JT K9AN R-12"
  char msg4[35] = "K9AN K1JT RRR"; // expected RX "K1JT K9AN 73"

  // mock CQ
  //char msg0[35] = "CQ K1JT FN20"; // mock RX to "CQ KN6ZDE CM87"
  //char msg2[35] = "K9AN K1JT +05"; // expected RX "K1JT K9AN R-12"
  //char msg4[35] = "K9AN K1JT RRR"; // expected RX "K1JT K9AN 73"

  if(txMsg == NULL) return false;

  if(txMsg->status == MSG_SENT) {
    // add mock reply
    switch(txNextMsg) {
      case QSO_MSG_0:
        AddRxResponse(msg1, ft8RxFreq, &tmSlot, 148);
        txQueue[QSO_MSG_2] = AddTxMsg(msg2, ft8TxFreq, 78.0);
        txQueue[QSO_MSG_4] = AddTxMsg(msg4, ft8TxFreq, 78.0);
        txMsg->status = MSG_ACK;
        break;

      case QSO_MSG_1:
        break;

      case QSO_MSG_2:
        AddRxResponse(msg3, ft8RxFreq, &tmSlot, 148);
        txMsg->status = MSG_ACK;
        break;

      case QSO_MSG_3:
        break;

      case QSO_MSG_4:
        AddRxResponse(msg5, ft8RxFreq, &tmSlot, 148);
        txQueue[QSO_MSG_0]->status = MSG_COMPLETED;
        txQueue[QSO_MSG_2]->status = MSG_COMPLETED;
        txMsg->status = MSG_COMPLETED;
        break;

      case QSO_MSG_5:
        break;
    }

    switch(txNextMsg) {
      case QSO_MSG_0:
        txNextMsg = QSO_MSG_2;
        break;

      case QSO_MSG_1:
        break;

      case QSO_MSG_2:
        txNextMsg = QSO_MSG_4;
        break;

      case QSO_MSG_3:
        break;

      case QSO_MSG_4:
        ChangeFt8TxState(1); // turn FT8 transmission off
        result = false;
        break;

      case QSO_MSG_5:
        break;
    }

    DisplayAllMessages();
  }
/*
  switch(txMsg->status) {
    case MSG_WAITING:
      break;

    case MSG_NEXT:
      break;

    case MSG_SENT:
      break;

    case MSG_ACK:
      break;

    case MSG_TIMEOUT:
      break;

    case MSG_COMPLETED:
      break;
  }
*/

  return result;
}
#endif

//-------------------------------------------------------------------------------------------------------------
// Internal FT8 Code - Sync and Message Lists
//-------------------------------------------------------------------------------------------------------------

void AutoSyncFT8() {
  TOGGLEPROFILEPIN(PROFILER_FT8DECODE_PIN);

  // allow process to loop until we're within 1 second of the next T/R sequence
  if((second())%15 == 14) {
    SETPROFILEPIN(PROFILER_FT8DECODE_PIN);

    // now we can sync up without causing a long delay
    while((second())%15 != 0) {
      YieldToProcess();
      //Q_in_L.clear();
      //Q_in_R.clear();
    }

    start_time =millis();
    ft8SyncState = 1;

    RESETPROFILEPIN(PROFILER_FT8DECODE_PIN);
  }
  else {
    ft8SyncState = 0;
    if((Q_in_L.available() > 100) && (Q_in_R.available() > 100)) {
      DEBUG_MEM("clearing @ auto sync...");
      Q_in_L.clear();
      Q_in_R.clear();
    }
  }

  UpdateInfoBoxItem(IB_ITEM_FT8);
}

// add a message index to the specified message list
void AddMsg(int *list, int index, int &msgCount, int &listHead, int max, bool (&func)(RxMsg*)) {
  if(func(&rxBuf[index])) {
    // head points to most recently added msg
    // check if next addition is past end of list
    if(++listHead >= max) {
      listHead = 0; // roll it
    }

    list[listHead] = index;

    if(++msgCount > max) {
      msgCount = max;
    }
  }
}

bool AllMsgCheck(RxMsg *msg) {
  // *** TODO: impliment filters
  return true;
}

bool CqMsgCheck(RxMsg *msg) {
  return strcmp(msg->field1, "CQ") == 0;
}

bool RxMsgCheck(RxMsg *msg) {
  return (msg->freq >= ft8RxFreq - 10.0) && (msg->freq <= ft8RxFreq + 10.0);
}

void AddMsg(int index, int window) {
  switch(window) {
    case ALL_WINDOW:
      AddMsg(allList, index, allMsgs, allHead, MAX_LIST_MESSAGES, AllMsgCheck);
      break;

    case CQ_WINDOW:
      AddMsg(cqList, index, cqMsgs, cqHead, MAX_LIST_MESSAGES, CqMsgCheck);
      break;

    case RX_WINDOW:
      AddMsg(rxList, index, rxMsgs, rxHead, MAX_LIST_MESSAGES, RxMsgCheck);
      break;
  }
}

void AddMsgs(int index) {
  AddMsg(index, ALL_WINDOW);
  AddMsg(index, CQ_WINDOW);
  AddMsg(index, RX_WINDOW);
}

void AddDecodedMessage(struct tm *tmSlot, int16_t score, float time_sec, float freq, char *msg) {
  // update decoded msg detail
  strncpy(rxBuf[decodedMsgs].msg, msg, 35);
  rxBuf[decodedMsgs].msg[34] = '\0'; // ensure msg is terminated (only needed w/ possible decode error)
  rxBuf[decodedMsgs].freq = freq;
  rxBuf[decodedMsgs].slot_time.tm_hour = tmSlot->tm_hour;
  rxBuf[decodedMsgs].slot_time.tm_min = tmSlot->tm_min;
  rxBuf[decodedMsgs].slot_time.tm_sec = tmSlot->tm_sec;
  rxBuf[decodedMsgs].time_sec = time_sec;
  GetTeensyTime();
  rxBuf[decodedMsgs].hour = hour();
  rxBuf[decodedMsgs].min = minute();
  rxBuf[decodedMsgs].sec = second();
  rxBuf[decodedMsgs].sync_score = score;
  rxBuf[decodedMsgs].snr = (score - 160.0) / 6.0; // *** TODO: evaluate ft8_lib for better algorithm ***
  //rxBuf[decodedMsgs].distance = CalcLocatorDistance(text);

  // split msg into fields for use in automated routines
  // *** this doesn't cover all message types ***
  strncpy(rxBuf[decodedMsgs].field1, strtok(msg, " "), 20);
  strncpy(rxBuf[decodedMsgs].field2, strtok(NULL, " "), 20);
  strncpy(rxBuf[decodedMsgs].field3, strtok(NULL, " "), 20);

  //Serial.println(rxBuf[decodedMsgs].field1);
  //Serial.println(rxBuf[decodedMsgs].field2);
  //Serial.println(rxBuf[decodedMsgs].field3);

  AddMsgs(decodedMsgs); // add messages to window lists

  // update msg count
  ++decodedMsgs;
  if(decodedMsgs >= MAX_DECODED_MESSAGES) {
    decodedMsgs = 0; // start overwriting older messages
  }
}

// *** TODO: this should take call and report ***
TxMsg *AddTxMsg(char *msg, float freq, float  snr) {
  TxMsg *txMsg = &txBuf[txMsgs++];

  // update decoded msg detail
  strncpy(txMsg->msg, msg, 35);
  txMsg->msg[34] = '\0'; // ensure msg is terminated (only needed w/ possible decode error)
  txMsg->freq = freq; // *** will be updated immediately prior to transmission ***
  //txMsg->slot_time.tm_hour = tmSlot->tm_hour;
  //txMsg->slot_time.tm_min = tmSlot->tm_min;
  //txMsg->slot_time.tm_sec = tmSlot->tm_sec;
  //txMsg->time_sec = time_sec;
  //GetTeensyTime();
  //rxBuf[index].hour = hour();
  //rxBuf[index].min = minute();
  //rxBuf[index].sec = second();
  txMsg->snr = snr;

  // split msg into fields for use in automated routines
  // *** this doesn't cover all message types ***
  strncpy(txMsg->field1, strtok(msg, " "), 20);
  strncpy(txMsg->field2, strtok(NULL, " "), 20);
  strncpy(txMsg->field3, strtok(NULL, " "), 20);

  txMsg->status = MSG_WAITING;
  txMsg->tries = 0; // incremented to 0 on first transmission

  return txMsg;
}

// reset list
void ResetList(int window) {
  switch(window) {
    case ALL_WINDOW:
      allMsgs = 0;
      allHead = -1;
      allTop = 0;
      allScroll = false;
      break;

    case CQ_WINDOW:
      cqMsgs = 0;
      cqHead = -1;
      cqTop = 0;
      cqScroll = false;
      break;

    case RX_WINDOW:
      rxMsgs = 0;
      rxHead = -1;
      rxTop = 0;
      rxScroll = false;
      break;
  }
}

void CreateList(int window) {
  ResetList(window);

  for(int i = 0; i < decodedMsgs; i++) {
    AddMsg(i, window);
  }
}

void DisplaySelectedMessageDetail() {
  char message[100];
  RxMsg *msg = &rxBuf[activeMsg];
  tft.setFontScale((enum RA8875tsize)0);
  int rowHeight = tft.getFontHeight();

  if(decodedMsgs > 0) {
    // erase old info
    tft.fillRect(WATERFALL_L, FT8_MSG_WINDOW_DETAIL, WATERFALL_W, rowHeight, RA8875_BLACK);

    // display active message detail
        //printf("%02d%02d%02d %+05.1f %+4.2f %4.0f ~  %s\n",
        //    tm_slot_start->tm_hour, tm_slot_start->tm_min, tm_slot_start->tm_sec,
        //    snr, time_sec, freq, text);
    tft.setTextColor(YELLOW);
    tft.setCursor(WATERFALL_L, FT8_MSG_WINDOW_DETAIL - 3);
    //tft.print("Selected msg detail: ");
    if(msg->time_sec >0) {
      sprintf(message, "%02d%02d%02d %+4d +%d.%1d %4d ~  %s", msg->slot_time.tm_hour, msg->slot_time.tm_min, msg->slot_time.tm_sec,
        (int)msg->snr, (int)msg->time_sec, (int)(msg->time_sec * 10.0 - ((int)msg->time_sec) * 10.0), (int)msg->freq, msg->msg);
    } else {
      sprintf(message, "%02d%02d%02d %+4d -%d.%1d %4d ~  %s", msg->slot_time.tm_hour, msg->slot_time.tm_min, msg->slot_time.tm_sec,
        (int)msg->snr, (int)abs(msg->time_sec), (int)abs((msg->time_sec * 10.0 - ((int)msg->time_sec) * 10.0)), (int)msg->freq, msg->msg);
    }
    tft.print(message);
  }
}

void DisplayStats(int window, int num, int top, int head, bool scroll) {
  int rowHeight, colWidth, columnOffset;
  bool up = num > FT8_MSG_ROWS ? true : false;
  bool down = up;

  tft.setFontScale((enum RA8875tsize)0);
  rowHeight = tft.getFontHeight();
  colWidth = tft.getFontWidth();
  columnOffset = colWidth * 21 * window;

  // reset message area
  tft.fillRect(columnOffset, FT8_MSG_LIST_SUMMARY, colWidth * 21, rowHeight, RA8875_BLACK);

  tft.setTextColor(YELLOW);
  tft.setCursor(WATERFALL_L + columnOffset, FT8_MSG_LIST_SUMMARY);
  switch(window) {
    case ALL_WINDOW:
      tft.print("All(");
      break;

    case CQ_WINDOW:
      tft.print("CQ(");
      break;

    case RX_WINDOW:
      tft.print("RX(");
      break;
  }

  tft.print(num);
  tft.print(")   ");
  if(scroll) {
    tft.setTextColor(RA8875_GREEN);
  } else {
    tft.setTextColor(WHITE);
  }

  if(top == 0) up = false;
  //if(top >= num-FT8_MSG_ROWS) down = false;
  if(top >= head - FT8_MSG_ROWS + 1) down = false;
  if(up) tft.write(30); // scroll up pointer
  if(down) tft.write(31); // scroll down pointer
}

void DisplayListStats(int window) {
  switch(window) {
    case ALL_WINDOW:
      DisplayStats(window, allMsgs, allTop, allHead, allScroll);
      break;

    case CQ_WINDOW:
      DisplayStats(window, cqMsgs, cqTop, cqHead, cqScroll);
      break;

    case RX_WINDOW:
      DisplayStats(window, rxMsgs, rxTop, rxHead, rxScroll);
      break;
  }
}

// window: 0: all, 1: CQ, 2: RX
void DisplayMessages(int window, int *list, int numMsgs, bool scroll, int &top, int head, int max) {
  char message[100];
  int rowHeight, colWidth, columnOffset;
  int count = 0; // count of rows displayed
  int i, index;

  tft.setFontScale((enum RA8875tsize)0);
  rowHeight = tft.getFontHeight();
  colWidth = tft.getFontWidth();
  columnOffset = colWidth * 21 * window;

  // reset message area
  tft.fillRect(columnOffset, FT8_MSG_LIST_TOP, colWidth * 21, rowHeight * FT8_MSG_ROWS, RA8875_BLACK);

  if(numMsgs > 0) {
    // set msg window top if not scrolling
    if(!scroll) {
      if(numMsgs > FT8_MSG_ROWS) {
        top = head - FT8_MSG_ROWS + 1;
        if(top < 0) {
          top += max;
        }
      } else {
        top = 0;
      }
    }

    i = top;

    // print recent messages
    while(count < numMsgs) {
      if(count >= FT8_MSG_ROWS || (qsoViewActive && (count + 1 >= FT8_MSG_ROWS))) break;

      index = list[i];
      //Serial.print(index); Serial.print(", "); Serial.println(activeMsg);
      if(index == activeMsg) {
        tft.setTextColor(YELLOW);
      } else {
        tft.setTextColor(RA8875_WHITE);
      }

      sprintf(message,"%.20s", rxBuf[index].msg);
      tft.setCursor(WATERFALL_L + columnOffset, FT8_MSG_LIST_TOP + rowHeight * count - 3);
      tft.print(message);

      if(i == head) break; // window rules (1) and (2)

      ++count;
      ++i;
      if(i >= max) i = 0;
    }
  }

  DisplayListStats(window);
}

// called on TX enable
void ResetTXMessages() {

}

// TX msg color:
//    White:  waiting
//    Green:  next to TX
//    Red:    no response previous interval
//    Yellow: sent/acknowledged/completed
int GetTxMsgColor (int index) {
  int color = WHITE; // waiting

  switch(txQueue[index]->status) {
    case MSG_NEXT:
      if(ft8TxState) color = RA8875_GREEN;
      break;

    case MSG_SENT:
    case MSG_ACK:
      color = YELLOW;
      break;

    case MSG_TIMEOUT:
      color = RED;
      break;

    case MSG_COMPLETED:
      color = YELLOW;
      break;
  }

  return color;
}

void DisplayTxMessages() {
  int rowHeight;

  tft.setFontScale((enum RA8875tsize)0);
  rowHeight = tft.getFontHeight();

  // erase old info
  tft.fillRect(WATERFALL_L, FT8_TX_QUEUE_TOP, WATERFALL_W, rowHeight, RA8875_BLACK);

  tft.setCursor(WATERFALL_L, FT8_TX_QUEUE_TOP - 3);
  //tft.print("TX Queue: ");

  if(txNextMsg == QSO_MSG_0 || txNextMsg == QSO_MSG_2 || txNextMsg == QSO_MSG_4) {
    if(txQueue[QSO_MSG_0] == NULL) return;

    // display CQ msg 0
    tft.setTextColor(GetTxMsgColor(0));
    tft.print(txQueue[0]->msg);
    if(txQueue[0]->tries > 1) {
      tft.print(" (");
      tft.print(txQueue[0]->tries);
      tft.print(")");
    }
  }
  if(txNextMsg == QSO_MSG_2 || txNextMsg == QSO_MSG_4) {
    if(txQueue[QSO_MSG_2] == NULL || txQueue[QSO_MSG_4] == NULL) return;

    tft.print("    ");

    // display CQ msgs 2, 4
    tft.setTextColor(GetTxMsgColor(2));
    tft.print(txQueue[2]->msg);

    tft.print("    ");

    tft.setTextColor(GetTxMsgColor(4));
    tft.print(txQueue[4]->msg);
  }
  if(txNextMsg == QSO_MSG_1) {
    // display CQ msgs 1, 3, 5
    tft.setTextColor(GetTxMsgColor(1));
    tft.setCursor(WATERFALL_L, FT8_TX_QUEUE_TOP - 3);
    tft.print(txQueue[1]->msg);

    tft.print("    ");

    tft.setTextColor(GetTxMsgColor(3));
    tft.print(txQueue[3]->msg);

    tft.print("    ");

    tft.setTextColor(GetTxMsgColor(5));
    tft.print(txQueue[5]->msg);
  }
}

void DisplayAllStats() {
  DisplayListStats(ALL_WINDOW);
  DisplayListStats(CQ_WINDOW);
  DisplayListStats(RX_WINDOW);
}

void DisplayAllMessages() {
  DisplayMessages(ALL_WINDOW, allList, allMsgs, allScroll, allTop, allHead, MAX_LIST_MESSAGES);
  DisplayMessages(CQ_WINDOW, cqList, cqMsgs, cqScroll, cqTop, cqHead, MAX_LIST_MESSAGES);
  DisplayMessages(RX_WINDOW, rxList, rxMsgs, rxScroll, rxTop, rxHead, MAX_LIST_MESSAGES);
  DisplaySelectedMessageDetail();
  DisplayTxMessages();
}

void ProcessFT8Messages() {
  // process incoming messages

  DisplayAllMessages();
}

//-------------------------------------------------------------------------------------------------------------
// Decoder State Code
//-------------------------------------------------------------------------------------------------------------

// set up TX messages for QSO type
// type: 0=CQ, 1= CQ reply
void InitQSO(int type, char *call, char *grid) {
  // *** TODO: use AddTxMsg(char *msg, int index, float freq, float  snr) ***
  char msg[35];

  if(call != NULL && grid != NULL) {
    if(type == 0) {
      // CQ
      sprintf(msg, "CQ %.13s %.6s", call, grid);

      txQueue[QSO_MSG_0] = AddTxMsg(msg, ft8TxFreq, 78.0);
    } else {
      // CQ reply

    }
  }
}

void InitDecoderState(char *call = NULL, char *grid = NULL) {
  // initialize message lists
  allTop = 0;
  cqTop = 0;
  rxTop = 0;
  decodedMsgs = 0;
  allMsgs = 0;
  cqMsgs = 0;
  rxMsgs = 0;
  allHead = -1;
  cqHead = -1;
  rxHead = -1;

  ft8SyncState = 0;
  ft8WavFlag = false;
  frameCount = 0;
  bufCount = 0;

  InitQSO(0, call, grid);

  ft8DecoderState = STATE_BUFFERING;
}

FLASHMEM bool InitFT8Decoder(const char *call, const char *grid) {
  bool result = false;

  if(ft8Init) {
    // FT8 decoder has already been initialized
    // just reset state and return success
    result = true;
  } else {
    // *** TODO: consider changing ft8lib monitor configuration when audio filters are changed ***
    if(ft8lib_InitDecoder()) {
      EraseSpectrumDisplayContainer();
      DrawSpectrumFrame();
      tft.writeTo(L2);
      tft.fillRect(SPECTRUM_LEFT_X, SPECTRUM_TOP_Y, SPECTRUM_RES, SPECTRUM_HEIGHT, RA8875_BLACK);
      tft.writeTo(L1);
      tft.fillRect(SPECTRUM_LEFT_X, SPECTRUM_TOP_Y, SPECTRUM_RES, SPECTRUM_HEIGHT, RA8875_BLACK);
      displayState = DISPLAY_T41_FT8_DECODE;
      ShowFT8SpectrumFreqValues();
      DrawFT8BandwidthBar();

      // update FT8 info box items
      infoBoxItemActive[IB_ITEM_FT8] = true;
      UpdateInfoBoxItem(IB_ITEM_FT8);
      infoBoxItemActive[IB_ITEM_FT8_TX] = true;
      UpdateInfoBoxItem(IB_ITEM_FT8_TX);
      infoBoxItemActive[IB_ITEM_FT8_TXF] = true;
      UpdateInfoBoxItem(IB_ITEM_FT8_TXF);
      infoBoxItemActive[IB_ITEM_FT8_RXF] = true;
      UpdateInfoBoxItem(IB_ITEM_FT8_RXF);
      infoBoxItemActive[IB_ITEM_FT8_INT] = true;
      UpdateInfoBoxItem(IB_ITEM_FT8_INT);
      infoBoxItemActive[IB_ITEM_FT8_CQ] = true;
      UpdateInfoBoxItem(IB_ITEM_FT8_CQ);

      // set up message area
      // Erase waterfall in decode area
      tft.fillRect(WATERFALL_L, FT8_WINDOW_TOP, WATERFALL_W, FT8_ROW_HEIGHT * FT8_ROWS + 3, RA8875_BLACK);
      tft.writeTo(L2); // it's on layer 2 as well
      tft.fillRect(WATERFALL_L, FT8_WINDOW_TOP, WATERFALL_W, FT8_ROW_HEIGHT * FT8_ROWS + 3, RA8875_BLACK);
      tft.writeTo(L1);
      wfRows = WATERFALL_H - FT8_ROW_HEIGHT * FT8_ROWS - 3;

      result = true;

      #ifdef USE_BUFFERED_FT8_WAV
      static bool done = false;

      if(!done) {
        // initialize buffered wav only once
        for(int i = 0; i < 15 * 12000; i++) {
          ft8WavBuf[i] = 0;
        }
        done = true;
      }
      #endif
    }
  }

  if(result) {
    //SetStationCoordinates(call);
    strncpy(baseCall, call, 14);
    strncpy(baseGrid, grid, 5);
    InitDecoderState(baseCall, baseGrid);
  }

  return result;
}

FLASHMEM void ExitFT8Decoder() {
  // return if the FT8 decoder hasn't been initialized
  if(!ft8Init) return;

  // ensure wav file is closed if we played one
  // *** TODO: consider separate wav exit ***
  CloseWav();

  // clean up ft8_lib
  ft8lib_ExitDecoder();

  displayState = DISPLAY_T41;

  // restore waterfall area
  tft.fillRect(WATERFALL_L, FT8_WINDOW_TOP, 512, FT8_ROW_HEIGHT * FT8_ROWS + 3, RA8875_BLACK);
  wfRows = WATERFALL_H;

  // redraw frequency spectrum area
  EraseSpectrumDisplayContainer();
  tft.writeTo(L2);
  EraseSpectrumDisplayContainer();
  tft.writeTo(L1);
  DrawSpectrumFrame();
  ShowSpectrumdBScale();
  ShowBandwidthBarValues();
  DrawBandwidthBar();
  ShowSpectrumFreqValues();

  // restore waterfall area

  // reset FT8 flags and counters
  InitDecoderState();

  // update FT8 info box items
  infoBoxItemActive[IB_ITEM_FT8] = false;
  infoBoxItemActive[IB_ITEM_FT8_TX] = false;
  infoBoxItemActive[IB_ITEM_FT8_TXF] = false;
  infoBoxItemActive[IB_ITEM_FT8_RXF] = false;
  infoBoxItemActive[IB_ITEM_FT8_INT] = false;
  infoBoxItemActive[IB_ITEM_FT8_CQ] = false;
  UpdateInfoBox();
}

FLASHMEM bool SetupFT8Wav() {
  int result;
  uint32_t slot_period = 15;
  uint32_t sample_rate = 12000;
  uint32_t num_samples = slot_period * sample_rate;

  //result = LoadWav("ft8.wav", num_samples); //
  //result = LoadWav("ft8_0.wav", num_samples); // 191111_110645.wav from ft8_lib
  //result = LoadWav("ft8_1.wav", num_samples); // CQ KN6ZDE CM87 at 1000
  //result = LoadWav("ft8_10.wav", num_samples); // CQ KN6ZDE CM8x x=0-9 at 1000 + x*100
  result = LoadWav("ft8_7.wav", num_samples); // CQ KN6ZDE CM8x x=0-6 at 500 + x*500

  if(result != 0) {
    DEBUG_MSG("Invalid wave file!");
    return false;
  }

  return true;
}

bool ReadFT8Wav(float32_t *buf, int sizeBuf) {
  bool result = ReadWav(buf, sizeBuf);

#ifdef USE_BUFFERED_FT8_WAV
  static bool bufInit = false;

  // transfer to wav buffer
  if(!bufInit) {
    for(int i = 0; i < sizeBuf && numWavBuf < 15*12000; i++, numWavBuf++) {
      ft8WavBuf[numWavBuf] = buf[i];
      //Serial.print(numWavBuf); Serial.print(", "); Serial.println(ft8WavBuf[numWavBuf]);
    }
    if(numWavBuf >= 15*12000-1) {
      bufInit = true;
      //Serial.print(numWavBuf);
    }
  }
#endif

  if(!result) {
    // done reading the wav file
    ft8WavFlag = true;
  }

  return result;
}

#ifdef USE_BUFFERED_FT8_WAV
bool ReadBufferedFT8Wav(float32_t *buf, int sizeBuf) {
  bool result = false;

  if((countWavBuf + sizeBuf) <= numWavBuf) {
    // transfer wav data to buffer
    for(int i = 0; i < sizeBuf; i++) {
      buf[i] = ft8WavBuf[countWavBuf + i];
    }
    result = true;
  } else {
    countWavBuf = countWavBufStart;
  }

  if(ft8SyncState) {
    countWavBuf += sizeBuf;
  }

  return result;
}
#endif

void BufferFT8Data(float *buf, int sizeBuf) {
  // don't buffer data until we're in sync
  if(ft8SyncState) {
    //Serial.print(bufCount); Serial.print(", "); Serial.println(frameCount);
    if(ft8lib_BufferSignal(buf, sizeBuf, frameCount * 1920 + bufCount * sizeBuf)) {
      ++bufCount;
      if(bufCount >= 15) {
        ft8DecoderState = STATE_PROCESSING;
        //bufCount = 0;
        //++frameCount;
      }
    }
  }
}

bool ProcessFT8Frame() {
  bool result = false;
  // process a frame of data
  // there are 79 frames in a FT8 message
  // but can be more within an interval depending on timing errors
  SETPROFILEPIN(PROFILER_FT8PROCESSBLOCK_PIN);
  // processing takes about 16ms
  if(ft8lib_ProcessFrame(frameCount)) {
    result = true;
  }
  RESETPROFILEPIN(PROFILER_FT8PROCESSBLOCK_PIN);

  return result;
}

void DecodeFT8Data(struct tm *start) {
  // long running process, stop queueing input
  Q_in_L.end();
  Q_in_R.end();
  Q_in_L.clear();
  Q_in_R.clear();

  // decode accumulated data (containing slightly less than a full time slot)
  // decode takes about:
  //   688ms with ft8_0.wav (~20 messages)
  //   530ms with ft8_7.wav (5 messages in FT8 range, 7 total)
  //   650ms with ft8_7.wav (5 messages in FT8 range, 7 total)
  //   650ms with ft8_7.wav buffered w/o offset
  //   575ms with ft8_7.wav buffered w/ 1 frame offset to (eliminate silence)
  // *** older measurements above are faster, why longer now? Mix of buffering and perhaps use of EXTMEM.
  // *** kFreq_osr and kTime_osr are 1 above vs 2 below
  //   850ms with ft8_7.wav (5 messages in FT8 range, 7 total)
  //   850ms with ft8_7.wav buffered w/ 1/2 frame offset to (eliminate some silence at beginning of signal)
  //   890ms with ft8_7.wav buffered w/ 1 frame offset to (eliminate silence)
  SETPROFILEPIN(PROFILER_FT8DECODE_PIN);
  ft8lib_Decode(start);
  RESETPROFILEPIN(PROFILER_FT8DECODE_PIN);

  // start input queues
  Q_in_L.begin();
  Q_in_R.begin();
}

// prep txQueue and TX state for RX messages
// returns false when a TX sequence is complete, otherwise true
bool RXProcessing() {
  bool result = true;

#ifdef TX_TESTING
  result = MockMsgTraffic() ;
#endif

  return result;
}

// prepare next TX
// assumes txQueue already initialized based on RX decodes
// returns true if TX can continue, false otherwise
bool TXPrep() {
  TxMsg *txMsg = txQueue[txNextMsg];
  bool result = false;

  if(txMsg->tries < 11) { // 10 tries
    txMsg->freq = ft8TxFreq;
    txMsg->status = MSG_NEXT;
    result = true;
  } else if(txMsg->status == 2 && ft8TxState) {
    // TX has been enabled again, reset msg status
    txMsg->status = MSG_NEXT;
    txMsg->tries = 0;
  } else {
    // msg transmitted 10 times, disable FT8 transmission
    txMsg->status = MSG_TIMEOUT;
    ChangeFt8TxState(1);
  }

  DisplayTxMessages();

  return result;
}

// wraps up TX process
void TXProcessing() {
  TxMsg *txMsg = txQueue[txNextMsg];

  txMsg->status = MSG_SENT;
  ++txMsg->tries;

  DisplayTxMessages();
}

/*
FT8 decoder state machine
FT8DecoderLoop is called from the main loop on DATA_RECEIVE_STATE. This loop performs one chunk of processing according to the
FT8 decoder state.  It then returns to the main loop to allow other radio operations to continue.  Similar to other transmission
states, a flag, ft8PTT is used to activate FT8 transmission in main loop. ft8PTT is set in this loop in STATE_TX which is set at
the top of an even/odd interval (as selected by ft8IntState) when FT8 transmission is enabled.  Transmission begins within 0.25
seconds (*** TODO: examine this further ***).

Loop Process States:
Buffering:
Forced sync (see below), data buffering begins on sync via call to YieldToProcess. Buffers 128 bytes of 12ksps audio for 15 loops
(1920 bytes total which is a frame of FT8 data or 1 symbol of the FT8 message 0.16 second long). YieldToProcess and
ShowAudioSpectrum called each pass regardless of sync.  After a frame of data, state is changed to Processing.

Processing:
Returns to Buffering state if not in sync.  Processes a frame of data with ft8lib, draws the frame frequency spectrum, increments
the frame counter, resets buffer counter and changes the state to Buffering or to Decoding when 79 frames have been processed.
(1920 byte frame * 79 = 151680 bytes or 0.16 * 79 = 12.64 second FT8 message, leaving 2.36 seconds for decoding and other processing).

Decoding:
Decodes buffered interval data with ft8lib, adds decoded messages to list, resets frame and buffer counts and sets RX Update state.
Input audio processing is paused and restarted as decoding is a long process.

RX Update:
Updates message windows with newly decoded messages and returns to Buffering state if FT8 transmission is not enabled. If FT8 transmission
is enabled: (1) returns to Buffering state if the next interval is not a transmission interval or otherwise flagged by TX prep process, or
(2) processes decoded messages and sets upcoming transmission message (see RX and TX Processing below) according to current transmission
state.  Input audio processing is then stopped, TX state is set and forced sync flagged.

TX:
Forced sync. On sync, message signal for interval is generated, ft8PTT set and state changed to TX Update.  TX occurs in main loop
where ft8PTT is reset.
(*** TODO: review timing profile, consider generating message signal in RX Update, but here is probably appropriate because TX freq can change ***)

TX Update:
Forced sync flagged, state changed to Buffering and input audio processing started.

Common routines:
Forced sync:
sync state set to false, call to AutoSyncFT8 allows returns to main loop for continued ops until 1 second before
next interval where YieldToProcess is called until start of interval and released to continue processing in this loop.
(*** TODO: 1 second sync lock ensures top of interval is caught but might be too restrictive for FT8 timing given decoding
takes up about 0.8 seconds of 2.36 seconds of remaining interval ***).

RX Processing:
Evaluates decoded messages for proper response to last transmission and advances TX state if appropriate.  Returns true
if QSO sequence is complete.

TX Prep:
Prepares for next transmission based on current TX state and status for current msg. TX for the current
state continues untiL the msg has been transmitted 10 times without reply, afterwhich, FT8 transmission
is disabled. TX can be continued by enabling FT8 transmission again.

TX Processing:
Updates tx message status for last transmission

*/
void FT8DecoderLoop() {
  struct tm tmSlotStart;

#ifdef SPECTRUM_TESTING
  // 0: update spectrum
  // 1: gathering one interval completed, updating paused
  // 2: examining captured frames possible with volume knob, exit by moving down one click from 0
  static int testingState = 0;
  static int frame = 0;
  static int vol = -1;

  // currently, the only way to exit testing is to restart the T41
  // draw spectrum frame in response to volume change
  if(testingState != 0) {
    if((audioVolume != vol) || (testingState == 1)) {
      //int tmp = audioVolume;
      frame += audioVolume - vol;

      if(frame >= 79) frame = 0;
      if(frame < 0) {
        frame = 0;
        tft.fillRect(WATERFALL_L, FT8_WINDOW_TOP, WATERFALL_W, FT8_ROW_HEIGHT * FT8_ROWS + 3, RA8875_BLACK);
        testingState = 0;
      } else {
        DrawTestSpectrum(freqSpectrum + frame * 512);
        tft.setTextColor(RA8875_GREEN);
        tft.setFontScale((enum RA8875tsize)1);
        tft.setCursor(10, SPECTRUM_BOTTOM+30);
        tft.print(frame);
      }

      if(testingState == 1) testingState = 2;

      // reset volume flag
      audioVolume = vol;
      UpdateInfoBoxItem(IB_ITEM_VOL);
    }
  }

#endif

  // *** TODO: examine various places that need input buffers cleared
  if((Q_in_L.available() > 100) && (Q_in_R.available() > 100)) {
    DEBUG_MEM("clearing @ start of FT8 decoder loop...");
    Q_in_L.clear();
    Q_in_R.clear();
  }

  switch(ft8DecoderState) {
    case STATE_BUFFERING:
      DEBUG_RXTX("at STATE_BUFFERING...");

      // ensure we're in sync
      if(ft8SyncState) {
        //UpdateFT8Synchronization();
        UpdateInfoBoxItem(IB_ITEM_FT8);
      } else {
        AutoSyncFT8();
      }

      // *** TODO: consider moving YieldToProcess call here to ShowAudioSpectrum as an option ***
      // *** normally the audio spectrum is updated once per frequency spectrum update
      //     but we have more time with wav file decoding ***
      // *** TODO: evaluate a single or multiple audio spectrum update(s) ***

      // about 130ms between frequency spectrum updates with either a single or multiple
      // audio spectrum update(s), therefore might as well do multiple updates
      // this analysis was made with an incomplete audio spectrum

      // with complete audio spectrum:
      // about 160ms between frequency spectrum updates with either a single or multiple
      // audio spectrum update(s), therefore might as well do multiple updates
      // with single update, spectrums are drawn in about 60ms
      // the loop just churns the rest of the time, 100ms, to capture remainder of frame, wasting processor

      if(bufCount < 15) {
        // gather multiple audio spectrums per ft8 interval
        // with this, audio spectrum is drawn before freq spectrum
        YieldToProcess(true);
        ShowAudioSpectrum();

        // single audio spectrum per ft8 interval
        //YieldToProcess();
        //if(ft8SpectrumFlag) {
        //  YieldToProcess(true);
        //  ShowAudioSpectrum();
        //  ft8SpectrumFlag = false;
        //}
      } else {
        // we have a frame of data, process it
        ft8DecoderState = STATE_PROCESSING;
      }
      break;

    case STATE_PROCESSING:
      if(!ft8SyncState) {
        // no need to waste time
        bufCount = 0;
        ft8DecoderState = STATE_BUFFERING;
        return;
      }

      DEBUG_LOC("at STATE_PROCESSING...");

      // a frame of data is available, process it
      // a frame of data is a single FT8 symbol (0.16 seconds of data)
      // 79 symbols per FT8 interval = 12.64 seconds
      // it is equivalent to:
      //  1920 bytes at 12k sample rate
      // it takes 15 buffer calls to fill a frame (128 * 15 = 1920)
      if(ProcessFT8Frame()) {
        // draw spectrum for previous frame
        uint8_t *spec = ft8lib_GetFT8SpectrumData(frameCount);

        if(spec != NULL) {
          DrawFT8Spectrum(spec, 512, frameCount == 38 || frameCount == 77);

          #ifdef SPECTRUM_TESTING
          if(testingState == 0) {
            // update frame spectrum
            for(int i = 0; i < 512; i++) {
              freqSpectrum[frameCount*512 + i] =  spec[i];
            }
          }
          #endif
        }

        // next frame
        // *** incrementing frame above moves to decoding one loop faster ***
        ++frameCount;
        // *** some portion of the next frame may already have been buffered
        //     just decrease buffer count by one frame size ***
        bufCount -= 15;

        if(frameCount < 79) {
          ft8DecoderState = STATE_BUFFERING;
        } else {
          #ifdef SPECTRUM_TESTING
          if(testingState == 0) {
            testingState = 1;
            vol = audioVolume;
          }
          #endif
          ft8DecoderState = STATE_DECODING;
        }

        #ifdef USE_BUFFERED_FT8_WAV
        // delay a bit since wav buffer playback is a bit fast
        if(currentDemodMode == DEMOD_FT8_INTERNAL) {
          //YieldForProcess(25);
        }
        #endif
      }
      break;

    case STATE_DECODING:
      DEBUG_LOC("at STATE_DECODING...");

      // FT8 interval completed, decode data
      // *** TODO: consider changes to allow continued signal processing while
      // we're within an interval, but after the normal 12.64 message window,
      // allowing for decoding of messages that start late ***
      tmSlotStart = { .tm_sec = (second() / 15) * 15, .tm_min = minute(), .tm_hour = hour() };
      //tmSlotStart = now();

      DecodeFT8Data(&tmSlotStart);

      frameCount = 0;
      bufCount = 0;

      ft8DecoderState = STATE_RX_UPDATE;
      break;

    case STATE_RX_UPDATE:
      DEBUG_LOC("at STATE_RX_UPDATE...");

      #ifndef SPECTRUM_TESTING
      // print messages if we're not testing
      ProcessFT8Messages();
      #endif

      #ifdef USE_BUFFERED_FT8_WAV
      // reset read wav buffer
      countWavBuf = countWavBufStart;
      UpdateInfoBoxItem(IB_ITEM_FT8);
      #endif

      ft8DecoderState = STATE_BUFFERING;

      if(ft8TxState) {
        // FT8 transmission is enabled

        // is next interval even?
        bool evenInterval = (((second() + 15) / 15) * 15) % 2 == 0;

        if((ft8IntState == 0 && evenInterval) || (ft8IntState == 1 && !evenInterval)) {
          // the next interval is a transmission interval

          // examine rxBuf for expected RX msgs
          if(RXProcessing()) {
            // we're at the start or within a transmission sequence

            // prepare next TX message
            if(TXPrep()) {

              // don't need to continue input queues
              Q_in_L.end();
              Q_in_R.end();
              Q_in_L.clear();
              Q_in_R.clear();

              ft8DecoderState = STATE_TX;
            }
          }

          SETPROFILEPIN(PROFILER_FT8GETDATA_PIN);
        }
      }

      // force a sync cycle
      ft8SyncState = 0;
      break;

    case STATE_TX:
      DEBUG_RXTX("at STATE_TX...");

      TOGGLEPROFILEPIN(PROFILER_FT8GETDATA_PIN);

      if(!ft8PTT) {
        // we continue looping through here until start of interval to transmit
        AutoSyncFT8();

        if(ft8SyncState) {
          if(ft8lib_GenFT8(txQueue[txNextMsg]->msg, txQueue[txNextMsg]->freq)) {
            // get msg signal and set FT8 PTT flag
            ft8TxSignalBuf = ft8lib_GetSignal();
            ft8PTT = true;
            ft8DecoderState = STATE_TX_UPDATE;
          } else {
            DEBUG_MSG("ft8lib_GenFT8 failed");
            ft8DecoderState = STATE_BUFFERING; // ensure return to buffering on error
          }
        }
      } else {
        // ft8PTT is only set to true above, when FT8 state is also advanced
        // *** should never be active with normal ops, but protects
        //     against an endless loop on a transmission glitch ***
        ft8DecoderState = STATE_BUFFERING;
      }
      break;

    case STATE_TX_UPDATE:
      DEBUG_LOC("at STATE_TX_UPDATE...");

      TXProcessing();

      ft8DecoderState = STATE_BUFFERING;

      // force a sync cycle
      ft8SyncState = 0;

      // start input queues
      Q_in_L.begin();
      Q_in_R.begin();

      RESETPROFILEPIN(PROFILER_FT8GETDATA_PIN);
      break;
  }

  if(ft8WavFlag) {
    // done with wav file
    // switch to FT8 internal mode
    ChangeMode(DATA_MODE, DEMOD_FT8_INTERNAL);
    InitDecoderState();
  }
}

//-------------------------------------------------------------------------------------------------------------
// Internal FT8 Code - User Input
//-------------------------------------------------------------------------------------------------------------

void ChangeFt8TxFreq(int wheel) {
  ft8TxFreq += wheel * ftIncrement;

  // limit it to spectrum range
  if(ft8TxFreq < 200) ft8TxFreq = 200; // bottom of ft8lib FT8 filter
  if(ft8TxFreq > 3350) ft8TxFreq = 3350; // 512 pixels * 6.25Hz/pixel + 200Hz offset - 50Hz bandwidth bar

  UpdateInfoBoxItem(IB_ITEM_FT8_TXF);

  DrawFT8BandwidthBar();
}

void ChangeFt8RxFreq(int wheel) {
  ft8RxFreq += wheel * ftIncrement;

  // limit it to spectrum range
  if(ft8RxFreq < 200) ft8RxFreq = 200; // bottom of ft8lib FT8 filter
  if(ft8RxFreq > 3350) ft8RxFreq = 3350; // 512 pixels * 6.25Hz/pixel + 200Hz offset - 50Hz bandwidth bar

  UpdateInfoBoxItem(IB_ITEM_FT8_RXF);

  DrawFT8BandwidthBar();
  CreateList(RX_WINDOW);
  DisplayMessages(RX_WINDOW, rxList, rxMsgs, rxScroll, rxTop, rxHead, MAX_LIST_MESSAGES);
}

void ChangeFt8TxInterval(int wheel) {
  ft8IntState += wheel;

  if(ft8IntState < 0) ft8IntState = 1;
  if(ft8IntState > 1) ft8IntState = 0;

  UpdateInfoBoxItem(IB_ITEM_FT8_INT);
}

void ChangeFt8CqState(int wheel) {
  ft8CqState += wheel;

  if(ft8CqState < 0) ft8CqState = 1;
  if(ft8CqState > 1) ft8CqState = 0;

  UpdateInfoBoxItem(IB_ITEM_FT8_CQ);
}

void ChangeFt8TxState(int wheel) {
  ft8TxState += wheel;

  if(ft8TxState < 0) ft8TxState = 1;
  if(ft8TxState > 1) ft8TxState = 0;

  UpdateInfoBoxItem(IB_ITEM_FT8_TX);

  // set message 0 status
  // *** TODO: consider TX state changes in other situations ***
  if(ft8TxState) {
    txQueue[QSO_MSG_0]->status = MSG_NEXT;
  } else {
    txQueue[QSO_MSG_0]->status = MSG_WAITING;
  }
  DisplayTxMessages();
}

int CalcTop(int top, int inc, int num, int head, int max) {
  int result = top - inc;

  if(result < 0) {
    // window top moved up beyond beginning of list
    if(num < max) {
      result = 0;
    } else {
      result += max;
      if(result < head) result = head;
    }
  } else if(result >= num) {
    // window top moved down beyond end of list
    if(num < max) {
      result = num - 1;
    } else {
      result = 0;
    }
  } else if(inc > 0) {
    // window moving up
    // top of window limited by head
    // can't move up to where top == head
    if((top > head) && (result == head)) result = top;
  } else if(inc < 0) {
    // window moving down
    // top of window limited by head
    // can't move down past top == head
    // (the display msg routine skips any messages in window below head)
    if((top == head) && (result > head)) result = head;
  }

  return result;
}

// scroll FT8 message window
// *** Note scrolling rules noted in the Data section ***
// wheel: >0: scroll up, <0: scroll down
void ScrollFt8MsgWindow(int xcol, int wheel) {
  if(xcol < 512 / 3) {
    if(allScroll) {
      // mouse in all messages
      allTop = CalcTop(allTop, wheel, allMsgs, allHead, MAX_LIST_MESSAGES);
      DisplayMessages(ALL_WINDOW, allList, allMsgs, allScroll, allTop, allHead, MAX_LIST_MESSAGES);
    }
  } else if(xcol > 512 * 2 / 3) {
    if(rxScroll) {
      // mouse in RX messages
      rxTop = CalcTop(rxTop, wheel, rxMsgs, rxHead, MAX_LIST_MESSAGES);
      DisplayMessages(RX_WINDOW, rxList, rxMsgs, rxScroll, rxTop, rxHead, MAX_LIST_MESSAGES);
    }
  } else {
    if(cqScroll) {
      // mouse in CQ messages
      cqTop = CalcTop(cqTop, wheel, cqMsgs, cqHead, MAX_LIST_MESSAGES);
      DisplayMessages(CQ_WINDOW, cqList, cqMsgs, cqScroll, cqTop, cqHead, MAX_LIST_MESSAGES);
    }
  }
}

int GetMsg(int x, int y) {
  int msgIndex = -1;
  // (YPIXELS - FT8_ROW_HEIGHT * FT8_ROWS) / FT8_ROW_HEIGHT = 17
  int row = ceil((float)y / 16.0 - 17.0 + 1) - 1;
  //Serial.print(y); Serial.print(", "); Serial.println(row);
  if(row < 0) return msgIndex;

  if(x < 512 / 3) {
    // mouse in all messages
    if(allTop + row <= allMsgs) {
      msgIndex = allTop + row - 1;
    }
  } else if(x > 512 * 2 / 3) {
    // mouse in RX messages
    if(rxTop + row <= rxMsgs) {
      msgIndex = rxList[rxTop + row - 1];
    }
  //} else if(x < 512 / 3) {
  } else {
    // mouse in CQ messages
    if(cqTop + row <= cqMsgs) {
      msgIndex = cqList[cqTop + row - 1];
    }
  }

  return msgIndex;
}

void ChangeFt8ActiveMsg(int x, int y) {
  int msgIndex = GetMsg(x, y);
  if(msgIndex < 0) return;

  activeMsg = msgIndex;

  DisplayAllMessages();
}

// toggle msg window scroll lock
void ChangeFt8ScrollLock(int x) {
  if(x < 512 / 3) {
    // mouse in all messages
    allScroll = !allScroll;
    DisplayListStats(ALL_WINDOW);
  } else if(x > 512 * 2 / 3) {
    // mouse in RX messages
    rxScroll = !rxScroll;
    DisplayListStats(RX_WINDOW);
  } else {
    // mouse in CQ messages
    cqScroll = !cqScroll;
    DisplayListStats(CQ_WINDOW);
  }
  //Serial.print(allScroll); Serial.print(", "); Serial.print(cqScroll); Serial.print(", "); Serial.println(rxScroll);
}

void CreateFt8TxMsg(int x, int y) {
  int msgIndex = GetMsg(x, y);
  if(msgIndex < 0) return;

  activeMsg = msgIndex;

  DisplayAllMessages();
}

void ToggleList(int x) {
  if(x < 512 / 3) {
    // mouse in all messages
    if(allScroll) {
      allTop = allTop < allMsgs ? allMsgs : 0;
      DisplayMessages(ALL_WINDOW, allList, allMsgs, allScroll, allTop, allHead, MAX_LIST_MESSAGES);
    }
  } else if(x > 512 * 2 / 3) {
    // mouse in RX messages
    if(rxScroll) {
    rxTop = rxTop < rxMsgs ? rxMsgs : 0;
    DisplayMessages(RX_WINDOW, rxList, rxMsgs, rxScroll, rxTop, rxHead, MAX_LIST_MESSAGES);
    }
  } else {
    // mouse in CQ messages
    if(cqScroll) {
      cqTop = cqTop < cqMsgs ? cqMsgs : 0;
      DisplayMessages(CQ_WINDOW, cqList, cqMsgs, cqScroll, cqTop, cqHead, MAX_LIST_MESSAGES);
    }
  }
}

void FT8MsgWindowClick(int x, int y, int button) {
  int row = ceil((float)y / 16.0 - 17.0 + 1);
  //Serial.print(y); Serial.print(", "); Serial.print(row); Serial.print(", "); Serial.println(button);
  if(row < 0) return;

  switch(row) {
    case 1:
      switch(button) {
        case 1: // left click
          //break;
        case 2: // right click
          ToggleList(x);
          break;
        case 4: // wheel click
          ChangeFt8ScrollLock(x);
          break;
      }
      break;

    default:
      switch(button) {
        case 1: // left click
          ChangeFt8ActiveMsg(x, y);
          break;
        case 2: // right click
          break;
        case 4: // wheel click
          CreateFt8TxMsg(x, y);
          break;
      }
      break;
  }
}
