
//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

extern bool catControlChange;

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

void SendBandChange(int upDown);
void SendBand(int band);
void SendCenterFreq(int freq);
void SendNCOFreq(int freq);
void SendFreqA(int freq);
void SendFreqB(int freq);
void SendMode(int mode);
void SendDemodMode(int mode);
void SendDisplayZoom(int zoom);
void SendSmeter(int smeterPad, float dbm);
void SendVolume(int volume);
void SendFilter();
void SendFilterHi(int filter);
void SendFilterLo(int filter);
void SendFreqIncrement(int index);
void SendFtIncrement(int index);
void SendMouseCenterTuneActive(int val);
void SendAGC(int val);
void SendNFSetting(int val);
void SendNoiseFloor(int val);

void SendSignalStrengthRequest();
void SendSignalStrengthRequest(int index);

void SendNarrowFilter();
