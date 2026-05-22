
#include "SDT.h"

#include "AudioConfig.h"
#include "t41Property.h"

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
void InitAMDemodBiquadFilter(int sampleRate);

int ProcessReceiverData(bool updateSpectrumData = false);
void ProcessControls();

void CalcZoomFreqSpec(uint32_t blockSize, bool updateSpectrumData);

void YieldToProcess(bool updateSpectrum = false);
void YieldForProcess(int ms);

inline int __attribute__((always_inline)) CheckReceiverData(bool updateSpectrumData = false) {
  // audio spectrum calc works with 256 samples which is 2 blocks at 44.1kHz or 16 blocks at 192kHz decimated by 8 or 24Hz
  int blocks = t41.DemodMode == DEMOD_FT8 ? 2 : 16;

  // *** TODO: we could streamline this further, there should always be the same number of blocks in the left and right channels.
  if((Q_in_L.available() >= blocks) && (Q_in_R.available() >= blocks)) {
    return ProcessReceiverData(updateSpectrumData);
  }

  return 0;
}
