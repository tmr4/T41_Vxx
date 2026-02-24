
#include <arm_math.h>
#include <arm_const_structs.h>

//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

extern arm_fir_decimate_instance_f32 FIR_dec1_I;
extern arm_fir_decimate_instance_f32 FIR_dec1_Q;
extern arm_fir_decimate_instance_f32 FIR_dec2_I;
extern arm_fir_decimate_instance_f32 FIR_dec2_Q;
extern arm_fir_decimate_instance_f32 FIR_dec3_I; // internal FT8 decode
extern arm_fir_decimate_instance_f32 FIR_dec3_Q; // internal FT8 decode

extern arm_fir_interpolate_instance_f32 FIR_int1_I;
extern arm_fir_interpolate_instance_f32 FIR_int1_Q;
extern arm_fir_interpolate_instance_f32 FIR_int2_I;
extern arm_fir_interpolate_instance_f32 FIR_int2_Q;

extern arm_fir_decimate_instance_f32 FIR_dec1_EX_I;
extern arm_fir_decimate_instance_f32 FIR_dec1_EX_Q;
extern arm_fir_decimate_instance_f32 FIR_dec2_EX_I;
extern arm_fir_decimate_instance_f32 FIR_dec2_EX_Q;
extern arm_fir_interpolate_instance_f32 FIR_int1_EX_I;
extern arm_fir_interpolate_instance_f32 FIR_int1_EX_Q;
extern arm_fir_interpolate_instance_f32 FIR_int2_EX_I;
extern arm_fir_interpolate_instance_f32 FIR_int2_EX_Q;

extern arm_fir_decimate_instance_f32 FIR_dec3_EX_I;
extern arm_fir_decimate_instance_f32 FIR_dec3_EX_Q;
extern arm_fir_interpolate_instance_f32 FIR_int3_EX_I;
extern arm_fir_interpolate_instance_f32 FIR_int3_EX_Q;

extern arm_fir_instance_f32 FIR_Hilbert_L;
extern arm_fir_instance_f32 FIR_Hilbert_R;

extern arm_fir_instance_f32 FIR_CW_DecodeL;
extern arm_fir_instance_f32 FIR_CW_DecodeR;

// choose the size of the Hilbert vector (this also picks a set of coefficients, meaning filter design)
// I've chosen to use the older filter design.  See https://groups.io/g/SoftwareControlledHamRadio/message/34020 for a discussion.
//#define HILBERT_SIZE 128 // 12kHz sample rate, new coefficients
#define HILBERT_SIZE 256 // 24kHz sample rate, older coefficients
// Uncomment following line to use the older 24kHz Hilbert coefficients at that rate
// Keeping this commented uses the older Hilbert coefficients with newer 12kHz sample rate structure in PrepareExciterIQData
// see https://groups.io/g/SoftwareControlledHamRadio/message/34030
//#define USE_24K_SPS

extern float32_t FIR_Coef_I[256 + 1];
extern float32_t FIR_Coef_Q[256 + 1];

extern float32_t /* DMAMEM */ FIR_dec1_coeffs[27];
extern float32_t /* DMAMEM */ FIR_dec2_coeffs[33];
extern float32_t /* DMAMEM */ FIR_dec3_coeffs[33];
extern float32_t /* DMAMEM */ FIR_int1_coeffs[48];
extern float32_t /* DMAMEM */ FIR_int2_coeffs[32];

//-------------------------------------------------------------------------------------------------------------
// Code
//-------------------------------------------------------------------------------------------------------------

void InitFIRFilters();
void InitHilbertFilters();

void CalcFIRCoeffs(float *coeffs_I, int numCoeffs, float32_t fc, float32_t Astop, int type, float dfc, float Fsamprate);
void CalcCplxFIRCoeffs(float *coeffs_I, float *coeffs_Q, int numCoeffs, float32_t fLoCut, float32_t fHiCut, float sampleRate);
