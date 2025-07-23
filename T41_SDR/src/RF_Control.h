// v12 specific hardware file

//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

extern int currentRF_InAtten;
extern int currentRF_OutAtten;

extern int RAtten[];
extern int XAttenCW[];
extern int XAttenSSB[];

//-------------------------------------------------------------------------------------------------------------
// Code
//-------------------------------------------------------------------------------------------------------------

void InitRFControl();
void RFControl_Enable_Prescaler(bool status);
void SetRF_InAtten(int attenInx2);
void SetRF_OutAtten(int attenOutx2);

void printRFState();
