
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

#ifdef PROJECTSYSTEM
EXTMEM float32_t ft8WavBuf[15 * 12000]; // buffer a FT8 wav file for use with decoder
//int numWavBuf = 0, countWavBuf = 1920 * 14, countWavBufStart = 1920 * 14; // first 14 frames are zero, gives -0.8 sec offset vs +1.3 sec for original wave file
int numWavBuf = 0, countWavBuf = 1920 * 7, countWavBufStart = 1920 * 7; // first 14 frames are zero, gives +0.2 sec offset
//int numWavBuf = 0, countWavBuf = 0, countWavBufStart = 0;
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

//Decode *decoded;
//#define MAX_DECODED_MESSAGES 10 // for testing of decode list getting full
#define MAX_DECODED_MESSAGES 500
EXTMEM Decode decodedList[MAX_DECODED_MESSAGES];
#define MAX_LIST_MESSAGES 50
EXTMEM int rxList[MAX_LIST_MESSAGES], cqList[MAX_LIST_MESSAGES];

int numDecodedMsgs = 0, numRxMsgs = 0, numCqMsgs = 0;

int activeMsg = 0;

// message window message index
int cqWindowTop, allWindowTop, rxWindowTop;

uint32_t current_time, start_time, ft8_time;

int bufCount = 0;
int frameCount = 0;

//bool ft8SpectrumFlag; // true when ft8 frequency spectrum data is ready to be drawn
bool ft8WavFlag;      // true when ft8 wav file has closed (signals need to shift to DEMOD_FT8_DECODE mode)

int ft8_flag;

#define FT8_DECODER_STATE_BUFFERING   0
#define FT8_DECODER_STATE_PROCESSING  1
#define FT8_DECODER_STATE_DECODING    2
#define FT8_DECODER_STATE_RX_UPDATE   3
#define FT8_DECODER_STATE_TX          4
#define FT8_DECODER_STATE_TX_UPDATE   5

int ft8DecoderState = 0;

int ft8State = 0; // state status for info box: 0 - off, 1 - not sync'd, 2 - sync'd
int ft8TxState = 0; // ft8 state status for info box: 0 - off, 1 - on
int ft8IntState = 0; //  ft8 Tx interval state status for info box: 0 - even, 1 - odd
int ft8CqState = 0; // ft8 CQ response state: 0 - man, 1 - respond automatically to CQ

int master_offset, offset_step;

bool ft8Init = false;
bool ft8SyncFlag = false;

int ft8TxFreq = 1000;
int ft8RxFreq = 1000;

extern char myGrid[];

extern bool ft8PTT;

#define FT8_MSG_ROWS 11
#define FT8_ROWS 13
#define FT8_ROW_HEIGHT 16
#define FT8_COL_WIDTH 8

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
// Code
//-------------------------------------------------------------------------------------------------------------

void AutoSyncFT8() {
  // allow process to loop until we're within 1 second of the next T/R sequence
  if((second())%15 == 14) {
    // now we can sync up without causing a long delay
    while((second())%15 != 0){
      Q_in_L.clear();
      Q_in_R.clear();
    }

    start_time =millis();
    ft8_flag = 1;
    ft8SyncFlag = true;
    ft8State = 2;
    //displaySync("sync'd", RA8875_GREEN);

    if((Q_in_L.available() > 100) && (Q_in_R.available() > 100)) {
      Serial.println("clearing...");
      Q_in_L.clear();
      Q_in_R.clear();
    }
  }
  else {
    ft8State = 1;
  }

  UpdateInfoBoxItem(IB_ITEM_FT8);
}

//void sync_FT8() {
//  start_time =millis();
//  ft8_flag = 1;
//}

// called when ft8_flag = 0
void UpdateFT8Synchronization() {
  current_time = millis();
  ft8_time = current_time  - start_time;

  // we're missing every other interval, try to relax this a bit
  // are we within 3 sec of 15 sec interval
  if(ft8_time % 15000 <= 200)
  //if(ft8_flag == 0 && ft8_time % 15000 <= 266) { // within 4 sec of 15 sec interval
  {
    ft8_flag = 1;
  }
}

void AddDecodedMessage(struct tm *tmSlot, int16_t score, float time_sec, float freq, char *msg) {
  static int nextMsgSlot = 0;

  // update decoded msg detail
  strcpy(decodedList[nextMsgSlot].msg, msg);
  decodedList[nextMsgSlot].freq = freq;
  decodedList[nextMsgSlot].slot_time.tm_hour = tmSlot->tm_hour;
  decodedList[nextMsgSlot].slot_time.tm_min = tmSlot->tm_min;
  decodedList[nextMsgSlot].slot_time.tm_sec = tmSlot->tm_sec;
  decodedList[nextMsgSlot].time_sec = time_sec;
  GetTeensyTime();
  decodedList[nextMsgSlot].hour = hour();
  decodedList[nextMsgSlot].min = minute();
  decodedList[nextMsgSlot].sec = second();
  decodedList[nextMsgSlot].sync_score = score;
  decodedList[nextMsgSlot].snr = (score - 160.0) / 6.0; // *** TODO: evaluate ft8_lib for better algorithm ***
  //decodedList[nextMsgSlot].distance = CalcLocatorDistance(text);
  decodedList[nextMsgSlot].count = 1;

  ++nextMsgSlot;
  if(nextMsgSlot >= MAX_DECODED_MESSAGES) {
    nextMsgSlot = 0; // start overwriting older messages
  } else {
    ++numDecodedMsgs;
  }
}

void DisplayDetails(int msg, int row, int col) {
  char message[33];

  // reset message detail area
  //tft.fillRect(col, row + 20, INFO_BOX_W - 20, 20, RA8875_BLACK);
  tft.fillRect(col, row, 200, 20 - 1, RA8875_BLACK);

  sprintf(message,"%02i:%02i ", decodedList[msg].hour, decodedList[msg].min);

  tft.setCursor(col, row-1);
  //sprintf(&message[6],"%1d  %4.0f  %3d    %3d  %4d", (uint8_t)(decodedList[msg].sec / 15 + 1), decodedList[msg].freq, decodedList[msg].snr, decodedList[msg].sync_score, decodedList[msg].distance);
  sprintf(&message[6],"%1d  %4.0f  %3.0f  %4d", (uint8_t)(decodedList[msg].sec / 15 + 1), decodedList[msg].freq, decodedList[msg].snr, decodedList[msg].distance);
  //tft.print(message);
  //Serial.println(message);
}

void DisplayActiveMessageDetails() {
  char message[100];
  Decode *msg = &decodedList[activeMsg];
  tft.setFontScale((enum RA8875tsize)0);
  int rowHeight = tft.getFontHeight();

  if(numDecodedMsgs > 0) {
    // erase old info
    tft.fillRect(WATERFALL_L, YPIXELS - FT8_ROW_HEIGHT * 2, WATERFALL_W, FT8_ROW_HEIGHT, RA8875_BLACK);

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

void GetRxMessages() {
  numRxMsgs = 0;

  for(int i = 0; i < numDecodedMsgs; i++){
    if(numRxMsgs >= MAX_LIST_MESSAGES)
      break;

    if((decodedList[i].freq >= ft8RxFreq - 10.0) && (decodedList[i].freq <= ft8RxFreq + 10.0)) {
      rxList[numRxMsgs++] = i;
    }
  }
}

void DisplayRxMessages() {
  char message[100];
  int rowCount = FT8_MSG_ROWS;
  int columnOffset;
  int count = 0;

  if(numRxMsgs == 0) return;

  tft.setFontScale((enum RA8875tsize)0);
  int rowHeight = tft.getFontHeight();
  int colWidth = tft.getFontWidth();

  columnOffset = colWidth * 21 * 2;

  // reset RX message area
  tft.fillRect(columnOffset, YPIXELS - FT8_ROW_HEIGHT * FT8_ROWS, colWidth * 21, FT8_ROW_HEIGHT * FT8_MSG_ROWS, RA8875_BLACK);

  if(numRxMsgs > 11) {
    rxWindowTop = numRxMsgs - 11;
  }

  // print recent messages at RX frequency
  for(int i = rxWindowTop; i < numRxMsgs; i++){
    if(count >= FT8_MSG_ROWS)
      break;

      if(rxList[i] == activeMsg) {
      if(ft8MsgSelectActive) {
        tft.setTextColor(RA8875_GREEN);
      } else {
        tft.setTextColor(YELLOW);
      }
    } else {
      tft.setTextColor(RA8875_WHITE);
    }

    sprintf(message,"%.20s", decodedList[rxList[i]].msg);
    tft.setCursor(WATERFALL_L + columnOffset, YPIXELS - rowHeight * (rowCount + 2) - 3);
    tft.print(message);

    ++count;
    --rowCount;
  }
}

void GetCqMessages() {
  numCqMsgs = 0;

  for(int i = 0; i < numDecodedMsgs; i++){
    if(numCqMsgs >= MAX_LIST_MESSAGES)
      break;

    if((decodedList[i].msg[0] == 'C') && (decodedList[i].msg[1] == 'Q') && (decodedList[i].msg[2] == ' ')) {
      cqList[numCqMsgs++] = i;
    }
  }
}

void DisplayCqMessages() {
  char message[100];
  int rowCount = FT8_MSG_ROWS;
  int columnOffset;
  int count = 0;

  if(numCqMsgs == 0) return;

  tft.setFontScale((enum RA8875tsize)0);
  int rowHeight = tft.getFontHeight();
  int colWidth = tft.getFontWidth();

  columnOffset = colWidth * 21;

  // reset CQ message area
  tft.fillRect(columnOffset, YPIXELS - FT8_ROW_HEIGHT * FT8_ROWS, colWidth * 21, FT8_ROW_HEIGHT * FT8_MSG_ROWS, RA8875_BLACK);

  if(numCqMsgs > 11) {
    cqWindowTop = numCqMsgs - 11;
  }

  // print recent CQ messages
  for(int i = cqWindowTop; i < numCqMsgs; i++){
    if(count >= FT8_MSG_ROWS)
      break;

    if(cqList[i] == activeMsg) {
      if(ft8MsgSelectActive) {
        tft.setTextColor(RA8875_GREEN);
      } else {
        tft.setTextColor(YELLOW);
      }
    } else {
      tft.setTextColor(RA8875_WHITE);
    }

    sprintf(message,"%.20s", decodedList[cqList[i]].msg);
    tft.setCursor(WATERFALL_L + columnOffset, YPIXELS - rowHeight * (rowCount + 2) - 3);
    tft.print(message);

    ++count;
    --rowCount;
  }
}

void DisplayMessages() {
  char message[48];
  int rowCount = FT8_MSG_ROWS;
  int columnOffset;
  int count = 0;

  tft.setFontScale((enum RA8875tsize)0);
  int rowHeight = tft.getFontHeight();
  int colWidth = tft.getFontWidth();

  columnOffset = 0;

  // reset RX message area
  tft.fillRect(columnOffset, YPIXELS - FT8_ROW_HEIGHT * FT8_ROWS, colWidth * 21, FT8_ROW_HEIGHT * FT8_ROWS + 3, RA8875_BLACK);

  if(numDecodedMsgs > 11) {
    allWindowTop = numDecodedMsgs - 11;
  }

  // print most recent messages left column
  for(int i = 0; i < FT8_MSG_ROWS; i++){
    int x = allWindowTop + i; // message to display

    if((x >= numDecodedMsgs) || (x >= MAX_DECODED_MESSAGES))
      break;

    if(x == activeMsg) {
      if(ft8MsgSelectActive) {
        tft.setTextColor(RA8875_GREEN);
      } else {
        tft.setTextColor(YELLOW);
      }
    } else {
      tft.setTextColor(RA8875_WHITE);
    }

    sprintf(message,"%.20s", decodedList[x].msg);
    tft.setCursor(WATERFALL_L + columnOffset, YPIXELS - rowHeight * (rowCount + 2) - 3);
    tft.print(message);

    //allWindowMsgs[count] = i;

    ++count;
    if(count >= numDecodedMsgs)
      break;

    --rowCount;
    //if(rowCount == 0) {
    //  // move to next column
    //  rowCount = FT8_MSG_ROWS;
    //  columnOffset += colWidth * 21;
    //  if(columnOffset > 500) {
    //    columnOffset = 0;
    //  }
    //}
  }

  DisplayActiveMessageDetails();

  tft.setTextColor(RA8875_GREEN);
  tft.setCursor(WATERFALL_L, YPIXELS - rowHeight - 3);
  tft.print("CQ KN6ZDE CM87");
}

void DisplayAllMessages() {
  DisplayCqMessages();
  DisplayMessages();
  DisplayRxMessages();
}

void ProcessFT8Messages() {
  GetRxMessages();
  GetCqMessages();
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
    //ShowSpectrumFreqValues();
    DrawAudioSpectContainer();
    DrawAudioFilterLines();
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
    DrawAudioSpectContainer();
    DrawAudioFilterLines();
    //ShowSpectrumFreqValues();
    //ShowOperatingStats();
  }
}

FLASHMEM bool InitFT8Decoder() {
  bool result = false;

  // return true if the FT8 decoder has already been initialized
  if(ft8Init) return true;

  if(ft8lib_InitDecoder()) {
    EraseSpectrumDisplayContainer();
    DrawSpectrumFrame();
    tft.writeTo(L2);
    tft.fillRect(SPECTRUM_LEFT_X, SPECTRUM_TOP_Y, SPECTRUM_RES, SPECTRUM_HEIGHT, RA8875_BLACK);
    tft.writeTo(L1);
    tft.fillRect(SPECTRUM_LEFT_X, SPECTRUM_TOP_Y, SPECTRUM_RES, SPECTRUM_HEIGHT, RA8875_BLACK);
    displayState = DISPLAY_T41_FT8_DECODE;
    ShowFT8SpectrumFreqValues();

    //SetStationCoordinates(myGrid);

    // initialize message windows
    cqWindowTop = 0;
    allWindowTop = 0;
    rxWindowTop = 0;
    numDecodedMsgs = 0;
    numRxMsgs = 0;
    numCqMsgs = 0;

    //for(int i = 0; i < MAX_LIST_MESSAGES; i++) {
    //  rxList[i] = 0;
    //  cqList[i] = 0;
    //}

    ft8Init = true;
    ft8SyncFlag = false;
    //ft8SpectrumFlag = false;
    ft8WavFlag = false;
    frameCount = 0;
    bufCount = 0;

    // update FT8 info box items
    ft8State = 1; // not sync'd
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
  }

  return result;
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
    Serial.println("Invalid wave file!");
    return false;
  }

  return true;
}

FLASHMEM void ExitFT8Decoder() {
  // ensure wav file is closed if we played one
  // *** TODO: consider separate wav exit ***
  CloseWav();

  // clean up ft8_lib
  ft8lib_ExitDecoder();

  displayState = DISPLAY_T41;

  // redraw frequency spectrum area
  ShowSpectrumdBScale();
  ShowBandwidthBarValues();
  DrawBandwidthBar();
  ShowSpectrumFreqValues();

  // restore waterfall area
  tft.fillRect(WATERFALL_L, YPIXELS - 20 * 6, WATERFALL_W, FT8_ROW_HEIGHT * FT8_ROWS + 3, RA8875_BLACK);
  wfRows = WATERFALL_H;

  // reset FT8 flags and counters
  ft8Init = false;
  ft8SyncFlag = false;

  ft8_flag = 0;
  ft8State = 0;
  numDecodedMsgs = 0;
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

bool ReadFT8Wav(float32_t *buf, int sizeBuf) {
  bool result = ReadWav(buf, sizeBuf);

#ifdef PROJECTSYSTEM
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

  if(ft8SyncFlag) {
    countWavBuf += sizeBuf;
  }

  return result;
}

void BufferFT8Data(float *buf, int sizeBuf) {
  // don't buffer data until we're in sync
  if(ft8SyncFlag) {
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

  Q_in_L.clear();
  Q_in_R.clear();
}

// FT8 decoder state machine
void FT8DecoderLoop() {
  // *** TODO: this should be set per actual FT8 interval ***
  struct tm tmSlotStart; // = { .tm_sec = 45, .tm_min = 06, .tm_hour = 11 };
  char msg[] = "CQ KN6ZDE CM87";

  if(ft8SyncFlag) {
    //UpdateFT8Synchronization();
    UpdateInfoBoxItem(IB_ITEM_FT8);
  } else {
    AutoSyncFT8();
  }

  // *** TODO: examine various places that need input buffers cleared
  if((Q_in_L.available() > 100) && (Q_in_R.available() > 100)) {
    Serial.println("clearing...");
    Q_in_L.clear();
    Q_in_R.clear();
  }

  switch(ft8DecoderState) {
    case FT8_DECODER_STATE_BUFFERING:
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

        //bufCount = 0;
      }
      break;

    case FT8_DECODER_STATE_PROCESSING:
      // a frame of data is available, process it
      // a frame of data is a single FT8 symbol (0.16 seconds of data)
      // 79 symbols per FT8 interval = 12.64 seconds
      // it is equivalent to:
      //  1920 bytes at 12k sample rate
      // it takes 15 buffer calls to fill a frame (128 * 15 = 1920)
      if(ProcessFT8Frame()) {
        // next frame
        ++frameCount;
        // *** some portion of the next frame may already have been buffered
        //     just decrease buffer count by one frame size ***
        bufCount -= 15;

        if(frameCount < 79) {
          // draw spectrum for previous frame
          // *** incrementing frame above moves to decoding one loop faster ***
          uint8_t *spec = ft8lib_GetFT8SpectrumData(frameCount - 1);

          if(spec != NULL) {
            DrawFT8Spectrum(spec, 512, frameCount == 77);
          }

          ft8DecoderState = FT8_DECODER_STATE_BUFFERING;
        } else {
          ft8DecoderState = FT8_DECODER_STATE_DECODING;
        }

        #ifdef USE_BUFFERED_FT8_WAV
        // delay a bit since wav buffer playback is a bit fast
        if(bands[currentBand].demod == DEMOD_FT8_DECODE) {
          //YieldForProcess(25);
        }
        #endif
      }
      break;

    case FT8_DECODER_STATE_DECODING:
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
      ProcessFT8Messages();

      #ifdef PROJECTSYSTEM
      // reset read wav buffer
      countWavBuf = countWavBufStart;
      ft8SyncFlag = false;
      UpdateInfoBoxItem(IB_ITEM_FT8);
      #endif

      //ft8DecoderState = FT8_DECODER_STATE_BUFFERING;
      if(ft8TxState) {
        // is next interval even?
        bool evenInterval = (((second() + 15) / 15) * 15) % 2 == 0;

        if((ft8IntState == 0 && evenInterval) || (ft8IntState == 1 && !evenInterval)) {
          ft8DecoderState = FT8_DECODER_STATE_TX;
          ft8SyncFlag = false;
          SETPROFILEPIN(PROFILER_FT8GETDATA_PIN);
        } else {
          ft8DecoderState = FT8_DECODER_STATE_BUFFERING;
        }
      } else {
        ft8DecoderState = FT8_DECODER_STATE_BUFFERING;
      }

      // prepare next TX message

      break;

    case FT8_DECODER_STATE_TX:
      TOGGLEPROFILEPIN(PROFILER_FT8GETDATA_PIN);
      if(!ft8PTT) {
        AutoSyncFT8();

        if(ft8SyncFlag) {
          if(ft8lib_GenFT8(msg, 1000.0)) {
            ft8TxSignalBuf = ft8lib_GetSignal();
            ft8PTT = true;
          } else {
            Serial.println("ft8lib_GenFT8 failed");
          }
          ft8DecoderState = FT8_DECODER_STATE_TX_UPDATE;
        }
      } else {
        ft8DecoderState = FT8_DECODER_STATE_TX_UPDATE;
      }
      break;

    case FT8_DECODER_STATE_TX_UPDATE:
      // prepare expected RX messages
      ft8DecoderState = FT8_DECODER_STATE_BUFFERING;
      RESETPROFILEPIN(PROFILER_FT8GETDATA_PIN);
      break;
  }

  if(ft8WavFlag) {
    // done with wav file, switch to FT8 decode mode
    ChangeDemodMode(DEMOD_FT8_DECODE);
    //ChangeDemodMode(DEMOD_USB);
    ft8WavFlag = false;
  }
}

void ChangeFt8TxFreq(int wheel) {
  ft8TxFreq += wheel * ftIncrement;

  UpdateInfoBoxItem(IB_ITEM_FT8_TXF);
}

void ChangeFt8RxFreq(int wheel) {
  ft8RxFreq += wheel * ftIncrement;

  UpdateInfoBoxItem(IB_ITEM_FT8_RXF);

  GetRxMessages();
  DisplayRxMessages();
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

void ChangeFt8Window(int xcol, int wheel) {
  if(xcol < 512 / 3) {
    // mouse in all messages
    allWindowTop -= wheel;
    if(allWindowTop < 0) {
      allWindowTop = 0;
    }
    if(allWindowTop >= numDecodedMsgs) {
      allWindowTop = numDecodedMsgs - 1;
    }
    DisplayMessages();
  } else if(xcol > 512 * 2 / 3) {
    // mouse in RX messages
    rxWindowTop -= wheel;
    if(rxWindowTop < 0) {
      rxWindowTop = 0;
    }
    if(rxWindowTop >= numRxMsgs) {
      rxWindowTop = numRxMsgs - 1;
    }
    DisplayRxMessages();
  //} else if(xcol < 512 / 3) {
  } else {
    // mouse in CQ messages
    cqWindowTop -= wheel;
    if(cqWindowTop < 0) {
      cqWindowTop = 0;
    }
    if(cqWindowTop >= numCqMsgs) {
      cqWindowTop = numCqMsgs - 1;
    }
    DisplayCqMessages();
  }
}

void ChangeFt8ActiveMsg(int x, int y) {
  int row = (y - (YPIXELS - FT8_ROW_HEIGHT * FT8_ROWS)) / FT8_ROW_HEIGHT + 1;

  //Serial.println(row);

  if(row < 0) row = 0;

  if(x < 512 / 3) {
    // mouse in all messages
    if(allWindowTop + row < numDecodedMsgs) {
      //activeMsg = numDecodedMsgs - 1;
    //} else {
      activeMsg = allWindowTop + row;
    }
  } else if(x > 512 * 2 / 3) {
    // mouse in RX messages
    if(rxWindowTop + row < numRxMsgs) {
      activeMsg = rxList[rxWindowTop + row];
    }
  //} else if(x < 512 / 3) {
  } else {
    // mouse in CQ messages
    if(cqWindowTop + row < numCqMsgs) {
      activeMsg = cqList[cqWindowTop + row];
    }
  }

  DisplayAllMessages();
}
