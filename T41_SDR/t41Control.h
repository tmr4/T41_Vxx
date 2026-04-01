
//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

extern bool controlDataFlag;

extern bool signalStrengthReceived;
extern float signalStrength;
extern int signalStrengthReceivedIndex;

//-------------------------------------------------------------------------------------------------------------
// Code
//-------------------------------------------------------------------------------------------------------------

void T41ControlSetup();
void T41ControlLoop();

void T41PrepareSpectrumData(int16_t *data, int16_t max);
void T41ControlSendData(uint8_t *data, int len);

//void SendSmeter(int16_t smeterPad, float32_t dbm);
void SendSmeter(int smeterPad, float dbm);
void SendSetFreq(int freq);
void SendSetMode(int mode);
void SendSignalStrengthRequest();
void SendSignalStrengthRequest(int index);
void SendSetDisplayZoom(int zoom);
void SendSetNarrowFilter();
void SendSetBandChange(int upDown);
