
//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

extern int ft8SyncState;
extern int ft8TxFreq, ft8RxFreq, ft8TxState, ft8IntState, ft8CqState;
extern bool txEqualsRx;

extern float *ft8TxSignalBuf;

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
