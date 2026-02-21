
//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

extern int ft8_flag, ft8_decode_flag;
extern bool syncFlag;
extern int ft8State, ft8TxFreq, ft8RxFreq, ft8TxState, ft8IntState, ft8CqState;

extern bool ft8Init;

extern int DSP_Flag;

extern int numDecodedMsgs;

extern int FT_8_counter;

extern int activeMsg;

//-------------------------------------------------------------------------------------------------------------
// Code
//-------------------------------------------------------------------------------------------------------------

void DisplayAllMessages();
void DisplayActiveMessageDetails();

void update_synchronization();

bool SetupFT8();
bool SetupFT8Decode();
bool SetupFT8Wav();
void ExitFT8();

void auto_sync_FT8();

void ProcessFT8WaveData();
void BufferFT8Data(float *buffer_LTemp);
void ProcessFT8Messages();

void ChangeFt8TxFreq(int wheel);
void ChangeFt8RxFreq(int wheel);
void ChangeFt8TxInterval(int wheel);
void ChangeFt8CqState(int wheel);
void ChangeFt8TxState(int wheel);
void ChangeFt8Window(int xcol, int wheel);
void ChangeFt8ActiveMsg(int x, int y);
