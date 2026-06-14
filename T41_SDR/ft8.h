#pragma once

//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

// Message windows
#define ALL_WINDOW 0
#define CQ_WINDOW  1
#define RX_WINDOW  2
#define INFO_BOX   3

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

  uint8_t hour, min, sec;
  float time_sec;
  tm slot_time;
  bool evenInterval;

  int  sync_score;
  float  snr;
  //int  distance; // *** needs SetStationCoordinates in init ***
} RxMsg;

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

  // 0: waiting
  // 1: next to TX
  // 2: sent
  // 3: reply received/acknowledged
  // 4: no response after 10 tries (transmission disabled)
  // 5: completed
  int status;

  int tries;
} TxMsg;

typedef struct {
  int type; // 0: CQ, 1: CQ reply

  char call[20];

  // index 0,2,4 are calling CQ QSOs
  // index 1,3,5 are for replying to CQ
  int msg[6];

  // 0: waiting
  // 1: in progress
  // 2: completed
  // 3: abandoned
  int status;
} QsoView;

extern int ft8SyncState;
extern int ft8TxFreq, ft8RxFreq, ft8TxState, ft8IntState, ft8CqState;
extern bool txEqualsRx;

extern float *ft8TxSignalBuf;

extern bool ft8PTT;

//-------------------------------------------------------------------------------------------------------------
// Code
//-------------------------------------------------------------------------------------------------------------

bool InitFT8();
void ExitFT8();

bool InitFT8Decoder(const char *call, const char *grid);
bool SetupFT8Wav();
void ExitFT8Decoder();

bool ReadFT8Wav(float32_t *buf, int sizeBuf);
bool ReadBufferedFT8Wav(float32_t *buf, int sizeBuf);

void BufferFT8Data(float *buf, int sizeBuf);
void FT8DecoderLoop();

void ChangeFt8TxFreq(int wheel);
void ChangeFt8RxFreq(int wheel);
void ChangeFt8TxInterval(int wheel);
void ChangeFt8CqState(int wheel);
void ChangeFt8TxState(int wheel);
void ScrollFt8MsgWindow(int xcol, int wheel);
void FT8MsgWindowClick(int x, int y, int button);
