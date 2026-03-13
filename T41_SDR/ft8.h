
//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

extern int ft8SyncState;
extern int ft8TxFreq, ft8RxFreq, ft8TxState, ft8IntState, ft8CqState;
extern bool txEqualsRx;

extern bool ft8Init;

//extern int numDecodedMsgs;

extern int activeMsg;

extern float *ft8TxSignalBuf;

//-------------------------------------------------------------------------------------------------------------
// Code
//-------------------------------------------------------------------------------------------------------------

void DisplayAllMessages();
void DisplayActiveMessageDetails();

void UpdateFT8Synchronization();

bool InitFT8();
void ExitFT8();

bool InitFT8Decoder();
bool SetupFT8Wav();
void ExitFT8Decoder();

void AutoSyncFT8();

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
void ChangeFt8ActiveMsg(int x, int y);
void ChangeFt8ScrollLock(int x);
void CreateFt8TxMsg(int x, int y);
