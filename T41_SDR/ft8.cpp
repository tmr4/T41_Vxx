
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
//#define DEBUG_MEM(msg)
#define DEBUG_MEM(msg) \
  Serial.print(msg); Serial.print(": "); Serial.println(AudioMemoryUsageMax()); \
  AudioMemoryUsageMaxReset();


#ifdef USE_BUFFERED_FT8_WAV
EXTMEM float32_t ft8WavBuf[15 * 12000]; // buffer a FT8 wav file for use with decoder
//int numWavBuf = 0, countWavBuf = 1920 * 14, countWavBufStart = 1920 * 14; // first 14 frames are zero, gives -0.8 sec offset vs +1.3 sec for original wave file
//int numWavBuf = 0, countWavBuf = 1920 * 7, countWavBufStart = 1920 * 7; // first 14 frames are zero, gives +0.2 sec offset
int numWavBuf = 0, countWavBuf = 0, countWavBufStart = 0;
#endif

float *ft8TxSignalBuf = NULL;

#define RA8875_GREEN 0x07E0 // 0, 255, 0

typedef struct
{
  // three parts of FT8 message
  // *** put together with: sprintf(message,"%.13s %.13s %.6s",field1, field2, field3); ***
  char field1[20];
  char field2[20];
  char field3[20];

  char msg[35]; // FTX_MAX_MESSAGE_LENGTH = callsign[13] + space + callsign[13] + space + report[6] + terminator
  float  freq; // hz
  float time_sec;
  tm slot_time;

  //char decode_time[10];
  uint8_t hour, min, sec;
  int  sync_score;
  float  snr;
  int  distance;

  int count;
} Decode;

/*

Detailed Message List:
The decodedDetails list records the details of each decoded message.  It is sized to capture all
messages from an FT8 session.

FT8 Message Window:
The FT8 message window has three subwindows for (1) all messages, left, (2) CQ messages, middle,
and (3) messages around the RX frequency, right.  The subwindows are an FT8_MSG_ROWS row snapshot into the
associated message lists with the most recent messages toward the bottom of the list.  The windows are
scrollable with the index of the message at the top of the window as a global variable.

Subwindow Message Lists:
There is an integer list associated with each subwindow.  The integer is an index into the decodedDetails
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
EXTMEM Decode decodedDetails[MAX_DECODED_MESSAGES];

// msg window list size is a tradeoff of scrolling vs having old msg overwritten before desired
// having the allList separate from the details list allows a filter on the list (*** TODO: impliment ***)
//#define MAX_LIST_MESSAGES 50
#define MAX_LIST_MESSAGES 500
EXTMEM int allList[MAX_LIST_MESSAGES], rxList[MAX_LIST_MESSAGES], cqList[MAX_LIST_MESSAGES];

#define ALL_WINDOW 0
#define CQ_WINDOW 1
#define RX_WINDOW 2

int decodedMsgs = 0, decodedHead = -1;
int allMsgs, cqMsgs = 0, rxMsgs = 0; // number of msgs in each list
int allHead = -1, cqHead = -1, rxHead = -1; // head gets incremented prior to msg added to list
bool allScroll = false, cqScroll = false, rxScroll = false; // false: latest msgs shown, true: msg list scrollable
int allTop, cqTop, rxTop; // message window list top message index

int activeMsg = 0; // selected msg

uint32_t current_time, start_time, ft8_time;

#define FT8_MSG_ROWS 10
#define FT8_ROWS 13
#define FT8_ROW_HEIGHT 16
#define FT8_COL_WIDTH 8

int bufCount = 0;
int frameCount = 0;

//bool ft8SpectrumFlag; // true when ft8 frequency spectrum data is ready to be drawn
bool ft8WavFlag;      // true when ft8 wav file has closed (signals need to shift to DEMOD_FT8_INTERNAL mode)

#define FT8_DECODER_STATE_BUFFERING   0
#define FT8_DECODER_STATE_PROCESSING  1
#define FT8_DECODER_STATE_DECODING    2
#define FT8_DECODER_STATE_RX_UPDATE   3
#define FT8_DECODER_STATE_TX          4
#define FT8_DECODER_STATE_TX_UPDATE   5

int ft8DecoderState = 0;

int ft8TxState = 0; // ft8 state status for info box: 0 - off, 1 - on
int ft8IntState = 0; //  ft8 Tx interval state status for info box: 0 - even, 1 - odd
int ft8CqState = 0; // ft8 CQ response state: 0 - man, 1 - respond automatically to CQ

int master_offset, offset_step;

bool ft8Init = false;
int ft8SyncState = 0; // sync status: 0 - not sync'd, 1 - sync'd

int ft8TxFreq = 1000;
int ft8RxFreq = 1000;
bool txEqualsRx = true;

extern char myGrid[];

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

void PrepareFT8ExciterIQData(float *sig);

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
// Testing Code
//-------------------------------------------------------------------------------------------------------------

//#define DEMOD_FT8_TESTING // plots frame by frame frequency spectrum of selected wav file (change frame with volume knob)

#ifdef DEMOD_FT8_TESTING
EXTMEM uint8_t freqSpectrum[512*79];

FLASHMEM void DrawTestSpectrum(uint8_t *spec) {
  int yPlot, y1Plot = 0;

  // clear old spectrum
  //EraseSpectrumWindow();
  tft.fillRect(WATERFALL_L, YPIXELS - FT8_ROW_HEIGHT * FT8_ROWS, WATERFALL_W, FT8_ROW_HEIGHT * FT8_ROWS + 3, RA8875_BLACK);

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

//-------------------------------------------------------------------------------------------------------------
// Code
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
void AddMsg(int *list, int index, int &msgCount, int &listHead, int max, bool (&func)(Decode*)) {
  if(func(&decodedDetails[index])) {
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

bool AllMsgCheck(Decode *msg) {
  // *** TODO: impliment filters
  return true;
}

bool CqMsgCheck(Decode *msg) {
  return strcmp(msg->field1, "CQ") == 0;
}

bool RxMsgCheck(Decode *msg) {
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
  static int nextMsgSlot = 0;

  // update decoded msg detail
  strncpy(decodedDetails[nextMsgSlot].msg, msg, 35);
  decodedDetails[nextMsgSlot].msg[34] = '\0'; // ensure msg is terminated (only needed w/ possible decode error)
  decodedDetails[nextMsgSlot].freq = freq;
  decodedDetails[nextMsgSlot].slot_time.tm_hour = tmSlot->tm_hour;
  decodedDetails[nextMsgSlot].slot_time.tm_min = tmSlot->tm_min;
  decodedDetails[nextMsgSlot].slot_time.tm_sec = tmSlot->tm_sec;
  decodedDetails[nextMsgSlot].time_sec = time_sec;
  GetTeensyTime();
  decodedDetails[nextMsgSlot].hour = hour();
  decodedDetails[nextMsgSlot].min = minute();
  decodedDetails[nextMsgSlot].sec = second();
  decodedDetails[nextMsgSlot].sync_score = score;
  decodedDetails[nextMsgSlot].snr = (score - 160.0) / 6.0; // *** TODO: evaluate ft8_lib for better algorithm ***
  //decodedDetails[nextMsgSlot].distance = CalcLocatorDistance(text);
  decodedDetails[nextMsgSlot].count = 1;

  // split msg into fields for use in automated routines
  // *** this doesn't cover all message types ***
  strncpy(decodedDetails[nextMsgSlot].field1, strtok(msg, " "), 20);
  strncpy(decodedDetails[nextMsgSlot].field2, strtok(NULL, " "), 20);
  strncpy(decodedDetails[nextMsgSlot].field3, strtok(NULL, " "), 20);

  //Serial.println(decodedDetails[nextMsgSlot].field1);
  //Serial.println(decodedDetails[nextMsgSlot].field2);
  //Serial.println(decodedDetails[nextMsgSlot].field3);

  AddMsgs(nextMsgSlot); // add messages to window lists

  // update msg count
  ++nextMsgSlot;
  if(nextMsgSlot >= MAX_DECODED_MESSAGES) {
    nextMsgSlot = 0; // start overwriting older messages
  } else {
    ++decodedMsgs;
  }
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
  Decode *msg = &decodedDetails[activeMsg];
  tft.setFontScale((enum RA8875tsize)0);
  int rowHeight = tft.getFontHeight();

  if(decodedMsgs > 0) {
    // erase old info
    tft.fillRect(WATERFALL_L, YPIXELS - rowHeight * 2, WATERFALL_W, rowHeight, RA8875_BLACK);

    // display active message detail
        //printf("%02d%02d%02d %+05.1f %+4.2f %4.0f ~  %s\n",
        //    tm_slot_start->tm_hour, tm_slot_start->tm_min, tm_slot_start->tm_sec,
        //    snr, time_sec, freq, text);
    tft.setTextColor(YELLOW);
    tft.setCursor(WATERFALL_L, YPIXELS - rowHeight * 2 - 3);
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

void DisplayStats(int window, int num, bool scroll) {
  int rowHeight, colWidth, columnOffset;

  tft.setFontScale((enum RA8875tsize)0);
  rowHeight = tft.getFontHeight();
  colWidth = tft.getFontWidth();
  columnOffset = colWidth * 21 * window;

  // reset message area
  tft.fillRect(columnOffset, YPIXELS - rowHeight * FT8_ROWS, colWidth * 21, rowHeight, RA8875_BLACK);

  tft.setTextColor(YELLOW);
  tft.setCursor(WATERFALL_L + columnOffset, YPIXELS - rowHeight * FT8_ROWS);
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
  tft.print(")");
  if(scroll) {
    tft.setTextColor(RA8875_GREEN);
  } else {
    tft.setTextColor(WHITE);
  }
  tft.print(" Scroll");
}

void DisplayListStats(int window) {
  switch(window) {
    case ALL_WINDOW:
      DisplayStats(window, allMsgs, allScroll);
      break;

    case CQ_WINDOW:
      DisplayStats(window, cqMsgs, cqScroll);
      break;

    case RX_WINDOW:
      DisplayStats(window, rxMsgs, rxScroll);
      break;
  }
}

// window: 0: all, 1: CQ, 2: RX
void DisplaySubwindowMessages(int window, int *list, int numMsgs, bool scroll, int &top, int head, int max) {
  char message[100];
  int rowHeight, colWidth, columnOffset;
  int count = 0; // count of rows displayed
  int i, index;

  tft.setFontScale((enum RA8875tsize)0);
  rowHeight = tft.getFontHeight();
  colWidth = tft.getFontWidth();
  columnOffset = colWidth * 21 * window;

  // reset message area
  tft.fillRect(columnOffset, YPIXELS - rowHeight * (FT8_MSG_ROWS + 2), colWidth * 21, rowHeight * FT8_MSG_ROWS, RA8875_BLACK);

  if(numMsgs == 0) return;

  // set msg window top if not scrolling
  if(!scroll) {
    if(numMsgs > FT8_MSG_ROWS) {
      top = head - FT8_MSG_ROWS;
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
    if(count >= FT8_MSG_ROWS) break;

    index = list[i];
    if(index == activeMsg) {
      if(ft8MsgSelectActive) {
        tft.setTextColor(RA8875_GREEN);
      } else {
        tft.setTextColor(YELLOW);
      }
    } else {
      tft.setTextColor(RA8875_WHITE);
    }

    sprintf(message,"%.20s", decodedDetails[index].msg);
    tft.setCursor(WATERFALL_L + columnOffset, YPIXELS - rowHeight * (FT8_MSG_ROWS - count + 2) - 3);
    tft.print(message);

    if(i == head) break; // window rules (1) and (2)

    ++count;
    ++i;
    if(i >= max) i = 0;
  }

  DisplayListStats(window);
}

void DisplayOtherMessages() {
  int rowHeight = tft.getFontHeight();

  DisplaySelectedMessageDetail();

  tft.setTextColor(RA8875_GREEN);
  tft.setCursor(WATERFALL_L, YPIXELS - rowHeight - 3);
  tft.print("CQ KN6ZDE CM87");
}

void DisplayAllStats() {
  DisplayListStats(ALL_WINDOW);
  DisplayListStats(CQ_WINDOW);
  DisplayListStats(RX_WINDOW);
}

void DisplayAllMessages() {
  DisplaySubwindowMessages(ALL_WINDOW, allList, allMsgs, allScroll, allTop, allHead, MAX_LIST_MESSAGES);
  DisplaySubwindowMessages(CQ_WINDOW, cqList, cqMsgs, cqScroll, cqTop, cqHead, MAX_LIST_MESSAGES);
  DisplaySubwindowMessages(RX_WINDOW, rxList, rxMsgs, rxScroll, rxTop, rxHead, MAX_LIST_MESSAGES);
  DisplayOtherMessages();
}

void ProcessFT8Messages() {
  // process incoming messages

  DisplayAllMessages();
}

//-------------------------------------------------------------------------------------------------------------
// other
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

FLASHMEM bool InitFT8Decoder() {
  bool result = false;

  // return true if the FT8 decoder has already been initialized
  if(ft8Init) return true;

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

    //SetStationCoordinates(myGrid);

    // initialize message windows
    cqTop = 0;
    allTop = 0;
    rxTop = 0;
    decodedMsgs = 0;
    allMsgs = 0;
    cqMsgs = 0;
    rxMsgs = 0;
    decodedHead = -1;
    allHead = -1;
    cqHead = -1;
    rxHead = -1;

    //for(int i = 0; i < MAX_LIST_MESSAGES; i++) {
    //  rxList[i] = 0;
    //  cqList[i] = 0;
    //}

    ft8Init = true;
    ft8SyncState = 0;
    //ft8SpectrumFlag = false;
    ft8WavFlag = false;
    frameCount = 0;
    bufCount = 0;

    // update FT8 info box items
    ft8SyncState = 0; // not sync'd
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
    tft.fillRect(WATERFALL_L, YPIXELS - FT8_ROW_HEIGHT * FT8_ROWS, WATERFALL_W, FT8_ROW_HEIGHT * FT8_ROWS + 3, RA8875_BLACK);
    tft.writeTo(L2); // it's on layer 2 as well
    tft.fillRect(WATERFALL_L, YPIXELS - FT8_ROW_HEIGHT * FT8_ROWS, WATERFALL_W, FT8_ROW_HEIGHT * FT8_ROWS + 3, RA8875_BLACK);
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
  tft.fillRect(WATERFALL_L, YPIXELS - FT8_ROW_HEIGHT * FT8_ROWS, 512, FT8_ROW_HEIGHT * FT8_ROWS + 3, RA8875_BLACK);
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
  ft8Init = false;
  ft8SyncState = 0;

  decodedMsgs = 0;
  frameCount = 0;
  bufCount = 0;

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
        ft8DecoderState = FT8_DECODER_STATE_PROCESSING;
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

// FT8 decoder state machine
void FT8DecoderLoop() {
  // *** TODO: this should be set per actual FT8 interval ***
  struct tm tmSlotStart; // = { .tm_sec = 45, .tm_min = 06, .tm_hour = 11 };
  char msg[] = "CQ KN6ZDE CM87";

#ifdef DEMOD_FT8_TESTING
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
        tft.fillRect(WATERFALL_L, YPIXELS - FT8_ROW_HEIGHT * FT8_ROWS, WATERFALL_W, FT8_ROW_HEIGHT * FT8_ROWS + 3, RA8875_BLACK);
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
    case FT8_DECODER_STATE_BUFFERING:
      DEBUG_RXTX("at FT8_DECODER_STATE_BUFFERING...");

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
        ft8DecoderState = FT8_DECODER_STATE_PROCESSING;
      }
      break;

    case FT8_DECODER_STATE_PROCESSING:
      if(!ft8SyncState) {
        // no need to waste time
        bufCount = 0;
        ft8DecoderState = FT8_DECODER_STATE_BUFFERING;
        return;
      }

      DEBUG_LOC("at FT8_DECODER_STATE_PROCESSING...");

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

          #ifdef DEMOD_FT8_TESTING
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
          ft8DecoderState = FT8_DECODER_STATE_BUFFERING;
        } else {
          #ifdef DEMOD_FT8_TESTING
          if(testingState == 0) {
            testingState = 1;
            vol = audioVolume;
          }
          #endif
          ft8DecoderState = FT8_DECODER_STATE_DECODING;
        }

        #ifdef USE_BUFFERED_FT8_WAV
        // delay a bit since wav buffer playback is a bit fast
        if(currentDemodMode == DEMOD_FT8_INTERNAL) {
          //YieldForProcess(25);
        }
        #endif
      }
      break;

    case FT8_DECODER_STATE_DECODING:
      DEBUG_LOC("at FT8_DECODER_STATE_DECODING...");

      // FT8 interval completed, decode data
      // *** TODO: consider changes to allow continued signal processing while
      // we're within an interval, but after the normal 12.64 message window,
      // allowing for decoding of messages that start late ***
      tmSlotStart = { .tm_sec = (second() / 15) * 15, .tm_min = minute(), .tm_hour = hour() };
      //tmSlotStart = now();

      DecodeFT8Data(&tmSlotStart);

      frameCount = 0;
      bufCount = 0;

      ft8DecoderState = FT8_DECODER_STATE_RX_UPDATE;
      break;

    case FT8_DECODER_STATE_RX_UPDATE:
      DEBUG_LOC("at FT8_DECODER_STATE_RX_UPDATE...");

      #ifndef DEMOD_FT8_TESTING
      // print messages if we're not testing
      ProcessFT8Messages();
      #endif

      #ifdef USE_BUFFERED_FT8_WAV
      // reset read wav buffer
      countWavBuf = countWavBufStart;
      UpdateInfoBoxItem(IB_ITEM_FT8);
      #endif

      ft8DecoderState = FT8_DECODER_STATE_BUFFERING;

      if(ft8TxState) {
        // is next interval even?
        bool evenInterval = (((second() + 15) / 15) * 15) % 2 == 0;

        if((ft8IntState == 0 && evenInterval) || (ft8IntState == 1 && !evenInterval)) {
          ft8DecoderState = FT8_DECODER_STATE_TX;

          // don't need to continue input queues
          Q_in_L.end();
          Q_in_R.end();
          Q_in_L.clear();
          Q_in_R.clear();

          SETPROFILEPIN(PROFILER_FT8GETDATA_PIN);
        }

        // prepare next TX message
        // to come

      }

      // force a sync cycle
      ft8SyncState = 0;
      break;

    case FT8_DECODER_STATE_TX:
      DEBUG_RXTX("at FT8_DECODER_STATE_TX...");

      TOGGLEPROFILEPIN(PROFILER_FT8GETDATA_PIN);
      if(!ft8PTT) {
        // waiting until top of interval to transmit
        AutoSyncFT8();

        if(ft8SyncState) {
          if(ft8lib_GenFT8(msg, 1000.0)) {
            // get msg signal and set FT8 PTT flag
            ft8TxSignalBuf = ft8lib_GetSignal();
            ft8PTT = true;
          } else {
            DEBUG_MSG("ft8lib_GenFT8 failed");
          }
          ft8DecoderState = FT8_DECODER_STATE_TX_UPDATE;
        }
      } else {
        ft8DecoderState = FT8_DECODER_STATE_TX_UPDATE;
      }
      break;

    case FT8_DECODER_STATE_TX_UPDATE:
      DEBUG_LOC("at FT8_DECODER_STATE_TX_UPDATE...");

      // prepare for expected RX msgs
      // to come

      // prepare expected RX messages
      ft8DecoderState = FT8_DECODER_STATE_BUFFERING;

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
    ft8WavFlag = false;
  }
}

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
  DisplaySubwindowMessages(RX_WINDOW, rxList, rxMsgs, rxScroll, rxTop, rxHead, MAX_LIST_MESSAGES);
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
      DisplaySubwindowMessages(ALL_WINDOW, allList, allMsgs, allScroll, allTop, allHead, MAX_LIST_MESSAGES);
    }
  } else if(xcol > 512 * 2 / 3) {
    if(rxScroll) {
      // mouse in RX messages
      rxTop = CalcTop(rxTop, wheel, rxMsgs, rxHead, MAX_LIST_MESSAGES);
      DisplaySubwindowMessages(RX_WINDOW, rxList, rxMsgs, rxScroll, rxTop, rxHead, MAX_LIST_MESSAGES);
    }
  } else {
    if(cqScroll) {
      // mouse in CQ messages
      cqTop = CalcTop(cqTop, wheel, cqMsgs, cqHead, MAX_LIST_MESSAGES);
      DisplaySubwindowMessages(CQ_WINDOW, cqList, cqMsgs, cqScroll, cqTop, cqHead, MAX_LIST_MESSAGES);
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
    if(allTop + row <= decodedMsgs) {
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
    allTop = allTop < allMsgs ? allMsgs : 0;
    DisplaySubwindowMessages(ALL_WINDOW, allList, allMsgs, allScroll, allTop, allHead, MAX_LIST_MESSAGES);
  } else if(x > 512 * 2 / 3) {
    // mouse in RX messages
    rxTop = rxTop < rxMsgs ? rxMsgs : 0;
    DisplaySubwindowMessages(RX_WINDOW, rxList, rxMsgs, rxScroll, rxTop, rxHead, MAX_LIST_MESSAGES);
  } else {
    // mouse in CQ messages
    cqTop = cqTop < cqMsgs ? cqMsgs : 0;
    DisplaySubwindowMessages(CQ_WINDOW, cqList, cqMsgs, cqScroll, cqTop, cqHead, MAX_LIST_MESSAGES);
  }
}

void FT8MsgWindowClick(int x, int y, int button) {
  int row = ceil((float)y / 16.0 - 17.0 + 1);
  //Serial.print(y); Serial.print(", "); Serial.print(row); Serial.print(", "); Serial.println(button);
  if(row < 0) return;

  switch(button) {
    case 1: // left click
      if(row > 1) {
        ChangeFt8ActiveMsg(x, y);
      }
      break;
    case 2: // right click
      if(row == 1) {
        ChangeFt8ScrollLock(x);
      }
      break;
    case 4: // wheel click
      if(row == 1) {
        ToggleList(x);
      } else {
        CreateFt8TxMsg(x, y);
      }
      break;
  }
}
