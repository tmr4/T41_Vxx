
//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

extern float32_t /*DMAMEM*/ audioFFT[1024];
extern float32_t /*DMAMEM*/ audioIFFT[];

extern float32_t biquad_lowpass1_coeffs[];

extern uint8_t ANR_notch;
extern uint8_t ANR_notchOn;
extern int audioYPixel[];
extern float32_t audioMaxSquaredAve;

//-------------------------------------------------------------------------------------------------------------
// Code
//-------------------------------------------------------------------------------------------------------------

void InitFFTArrays();
void InitFFTFilter();
void InitAMDemodBiquadFilter();

void ZoomFFTPrep();

bool ProcessReceiverData(bool updateSpectrumData = false);
void ProcessControls();
float32_t CalcSignalStrength();
