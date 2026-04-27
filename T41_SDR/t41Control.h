
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

void SendCommand(int value, int id);

void SendBandChange(int upDown);
void SendFreqA(int freq);
void SendFreqB(int freq);
void SendSmeter(int smeterPad, float dbm);
void SendFilter();

void SendSignalStrengthRequest();
void SendSignalStrengthRequest(int index);

void SendNarrowFilter();
