
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

void SendSetBandChange(int upDown);
void SendSetFreq(int freq);
void SendSetMode(int mode);
void SendSetDisplayZoom(int zoom);
void SendSmeter(int smeterPad, float dbm);
void SendVolume();
void SendFilter();
void SendSetFineTune();

void SendSignalStrengthRequest();
void SendSignalStrengthRequest(int index);

void SendSetNarrowFilter();
