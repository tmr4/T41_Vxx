
//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

//extern float32_t /*DMAMEM*/ audioFFT[1024];
extern float32_t /*DMAMEM*/ audioFFT[];
extern float32_t /*DMAMEM*/ audioIFFT[];

extern float32_t /*DMAMEM*/ freqSpecBuf[1024];
extern float32_t /*DMAMEM*/ prevFreqSpecBuf[1024];

extern float32_t biquad_lowpass1_coeffs[];

extern uint8_t ANR_notch;
extern uint8_t ANR_notchOn;

extern float32_t audioSpectBuffer[]; // This can't be DMAMEM.  It will break the S-Meter.
extern float32_t audioMaxSquaredAve;

//-------------------------------------------------------------------------------------------------------------
// Code
//-------------------------------------------------------------------------------------------------------------

void InitFFTArrays();
void InitAMDemodBiquadFilter();

int ProcessReceiverData(bool updateSpectrumData = false);
void ProcessControls();

void CalcZoomFreqSpec(uint32_t blockSize, bool updateSpectrumData);

void YieldToProcess(bool updateSpectrum = false);
void YieldForProcess(int ms);
