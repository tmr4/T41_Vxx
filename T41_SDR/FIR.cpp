
#include "SDT.h"

#include "CWProcessing.h"
#include "Filter.h"
#include "FIR.h"
#include "pi.h"
#include "Utility.h"

#include "FIRcoef.h"

//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

uint8_t FIR_filter_window = 1; // 1 - 4-term Blackman-Harris, 2 - sine, 3 - cosine, 4 - Hann, other - Blackman-Nuttall

// receiver decimation/interpolation state and instance arrays
float32_t DMAMEM FIR_dec1_I_state[2074]; // numtaps+blocksize-1 = 27+2048-1 = 2074
float32_t DMAMEM FIR_dec1_Q_state[2074];
float32_t DMAMEM FIR_dec2_I_state[544]; // numtaps+blocksize-1 = 33+512-1 = 544
float32_t DMAMEM FIR_dec2_Q_state[544];
float32_t DMAMEM FIR_dec3_state[544]; // numtaps+blocksize-1 = 33+512-1 = 544

float32_t DMAMEM FIR_int1_I_state[279]; // (numTaps/L)+blockSize-1 = 48/2+256-1 = 279
float32_t DMAMEM FIR_int1_Q_state[279];
float32_t DMAMEM FIR_int2_I_state[519]; // (numTaps/L)+blockSize-1 = 32/4+512-1 = 519
float32_t DMAMEM FIR_int2_Q_state[519];

arm_fir_decimate_instance_f32 FIR_dec1_I;
arm_fir_decimate_instance_f32 FIR_dec1_Q;
arm_fir_decimate_instance_f32 FIR_dec2_I;
arm_fir_decimate_instance_f32 FIR_dec2_Q;
arm_fir_decimate_instance_f32 FIR_dec3;

arm_fir_interpolate_instance_f32 FIR_int1_I;
arm_fir_interpolate_instance_f32 FIR_int1_Q;
arm_fir_interpolate_instance_f32 FIR_int2_I;
arm_fir_interpolate_instance_f32 FIR_int2_Q;

float32_t DMAMEM FIR_dec1_coeffs[27];
float32_t DMAMEM FIR_dec2_coeffs[33];
float32_t DMAMEM FIR_dec3_coeffs[33];
float32_t DMAMEM FIR_dec3_2_coeffs[33];
float32_t DMAMEM FIR_int1_coeffs[48];
float32_t DMAMEM FIR_int2_coeffs[32];

// exciter decimation/interpolation state and instance arrays
float32_t DMAMEM FIR_dec1_EX_I_state[2095]; // numtaps+blocksize-1 = 48+2048-1 = 2095
float32_t DMAMEM FIR_dec1_EX_Q_state[2095];
float32_t DMAMEM FIR_dec2_EX_I_state[559]; // numtaps+blocksize-1 = 48+512-1 = 559
float32_t DMAMEM FIR_dec2_EX_Q_state[559];
float32_t DMAMEM FIR_dec3_EX_I_state[303]; // numtaps+blocksize-1 = 48+512-1 = 559
float32_t DMAMEM FIR_dec3_EX_Q_state[303];

float32_t DMAMEM FIR_int1_EX_I_state[279]; // (numTaps/L)+blockSize-1 = 48/2+256-1 = 279
float32_t DMAMEM FIR_int1_EX_Q_state[279];
float32_t DMAMEM FIR_int2_EX_I_state[523]; // (numTaps/L)+blockSize-1 = 48/4+512-1 = 523
float32_t DMAMEM FIR_int2_EX_Q_state[523];
float32_t DMAMEM FIR_int3_EX_I_state[151]; // (numTaps/L)+blockSize-1 = 48/2+256-1 = 279
float32_t DMAMEM FIR_int3_EX_Q_state[151];

arm_fir_decimate_instance_f32 FIR_dec1_EX_I;
arm_fir_decimate_instance_f32 FIR_dec1_EX_Q;
arm_fir_decimate_instance_f32 FIR_dec2_EX_I;
arm_fir_decimate_instance_f32 FIR_dec2_EX_Q;
arm_fir_decimate_instance_f32 FIR_dec3_EX_I;
arm_fir_decimate_instance_f32 FIR_dec3_EX_Q;

arm_fir_interpolate_instance_f32 FIR_int1_EX_I;
arm_fir_interpolate_instance_f32 FIR_int1_EX_Q;
arm_fir_interpolate_instance_f32 FIR_int2_EX_I;
arm_fir_interpolate_instance_f32 FIR_int2_EX_Q;
arm_fir_interpolate_instance_f32 FIR_int3_EX_I;
arm_fir_interpolate_instance_f32 FIR_int3_EX_Q;

//Hilbert FIR Filters
float32_t FIR_Hilbert_state_L[100 + 256 - 1];
float32_t FIR_Hilbert_state_R[100 + 256 - 1];

arm_fir_instance_f32 FIR_Hilbert_L;
arm_fir_instance_f32 FIR_Hilbert_R;

// CW decode Filters
float32_t FIR_CW_DecodeL_state[64 + 256 - 1];
float32_t FIR_CW_DecodeR_state[64 + 256 - 1];

arm_fir_instance_f32 FIR_CW_DecodeL;
arm_fir_instance_f32 FIR_CW_DecodeR;

float32_t DMAMEM FIR_Coef_I[256 + 1];
float32_t DMAMEM FIR_Coef_Q[256 + 1];

arm_fir_decimate_instance_f32 Fir_Zoom_FFT_Decimate_I1, Fir_Zoom_FFT_Decimate_Q1, Fir_Zoom_FFT_Decimate_I2, Fir_Zoom_FFT_Decimate_Q2;

// original CSDR taps
//#define ZOOM_TAPS_1 12
//#define ZOOM_TAPS_2 12

#define USE_NEW_FIR true

//#define ZOOM_TAPS_1 12
//#define ZOOM_TAPS_2 12
#define ZOOM_TAPS_1 13
#define ZOOM_TAPS_2 13
//#define ZOOM_TAPS_1 31
//#define ZOOM_TAPS_2 31
//#define ZOOM_TAPS_1 32
//#define ZOOM_TAPS_2 32
//#define ZOOM_TAPS_1 47
//#define ZOOM_TAPS_2 31

// all times measured at 2x
// USB test signals (kHz): 2x:1, 4x:25, 8x:37, 16x:43
// all tap combinations had USB edge signal splatter at 2x which continued to an extent until signal reached mid_USB
// large taps, like 100, just increase signal spread w/o flattening the spectrum or improving edge signal attenuation
// odd/even taps didn't make a difference, contrary to AI analysis of filter coefficient routine
// An extra environmental signal (not an alias of the test signal) shows up on my PS test bed at some zoom levels and may disappear or move with different taps
//#define ZOOM_TAPS_1  8
//#define ZOOM_TAPS_2 12
//#define ZOOM_TAPS_1 12 // CalcFreqSpecBuffered: 0.68ms, slightly more edge noise than 32/10
//#define ZOOM_TAPS_2 8
//#define ZOOM_TAPS_1 12 // CalcFreqSpecBuffered: 0.76ms, some aliasing with signals at spectrum edges
//#define ZOOM_TAPS_2 12
//#define ZOOM_TAPS_1 13 // CalcFreqSpecBuffered: 0.78ms
//#define ZOOM_TAPS_2 13
//#define ZOOM_TAPS_1 22 // CalcFreqSpecBuffered: 0.95ms
//#define ZOOM_TAPS_2 22
//#define ZOOM_TAPS_1 24 // CalcFreqSpecBuffered: 0.99ms
//#define ZOOM_TAPS_2 24
//#define ZOOM_TAPS_1 25 // CalcFreqSpecBuffered: 1.0ms
//#define ZOOM_TAPS_2 25
//#define ZOOM_TAPS_1 26 // CalcFreqSpecBuffered: 1.02ms
//#define ZOOM_TAPS_2 26
//#define ZOOM_TAPS_1 31 // CalcFreqSpecBuffered: 1.12ms
//#define ZOOM_TAPS_2 31
//#define ZOOM_TAPS_1 32 // CalcFreqSpecBuffered: 1.13ms
//#define ZOOM_TAPS_2 32
//#define ZOOM_TAPS_1 33
//#define ZOOM_TAPS_2 33
//#define ZOOM_TAPS_1 63 // CalcFreqSpecBuffered: 1.77ms
//#define ZOOM_TAPS_2 63
//#define ZOOM_TAPS_1 127 // CalcFreqSpecBuffered: 3.05ms
//#define ZOOM_TAPS_2 127

//#define ZOOM_TAPS_1 100 // CalcFreqSpecBuffered: 3.50ms
//#define ZOOM_TAPS_2 100
//#define ZOOM_TAPS_1 101 // spectrum the same as 100 taps (so no difference w/ odd taps)
//#define ZOOM_TAPS_2 101
//#define ZOOM_TAPS_1  32 // CalcFreqSpecBuffered: 1.8ms
//#define ZOOM_TAPS_2 100
//#define ZOOM_TAPS_1 32 // CalcFreqSpecBuffered: 0.88ms, less spread on edge signals than w/ 100 taps
//#define ZOOM_TAPS_2 10
//#define ZOOM_TAPS_1 10 // slightly higher edge noise floor
//#define ZOOM_TAPS_2 32
//#define ZOOM_TAPS_1 10 // more edge signal spread than 32/10 at 16x (also an additional environmental signal present)
//#define ZOOM_TAPS_2 100

// 16x testing w/ 43kHz test signal (USB 1kHz below top of spectrum)
// 4x USB test signal aliased in LSB at all zoom levels at 16/8 and 32/16 taps
//#define ZOOM_TAPS_1  8 // spectrum looks fine, but extra environmental signal at far end of low spectrum (not a signal alias)
//#define ZOOM_TAPS_2  4
//#define ZOOM_TAPS_1 16 // CalcFreqSpecBuffered: 0.73ms, extra signal with 8/4 is gone
//#define ZOOM_TAPS_2 8
//#define ZOOM_TAPS_1 32 // CalcFreqSpecBuffered: 0.73ms, slightly less midband signal attenuation and more alias attenuation
//#define ZOOM_TAPS_2 16
//#define ZOOM_TAPS_1 128 // no improvement over 16/8
//#define ZOOM_TAPS_2 64
//#define ZOOM_TAPS_1 127 // no improvement over 16/8
//#define ZOOM_TAPS_2 63

// size = numTaps + blockSize - 1
//float32_t DMAMEM Fir_Zoom_FFT_Decimate_I1_state[ZOOM_TAPS_1 + 2048 - 1] __attribute__((aligned(4)));
//float32_t DMAMEM Fir_Zoom_FFT_Decimate_Q1_state[ZOOM_TAPS_1 + 2048 - 1] __attribute__((aligned(4)));
//float32_t DMAMEM Fir_Zoom_FFT_Decimate_I2_state[ZOOM_TAPS_2 + 1024 - 1] __attribute__((aligned(4)));
//float32_t DMAMEM Fir_Zoom_FFT_Decimate_Q2_state[ZOOM_TAPS_2 + 1024 - 1] __attribute__((aligned(4)));
//float32_t DMAMEM Fir_Zoom_FFT_Decimate1_coeffs[ZOOM_TAPS_1] __attribute__((aligned(4)));
//float32_t DMAMEM Fir_Zoom_FFT_Decimate2_coeffs[ZOOM_TAPS_2] __attribute__((aligned(4)));

#define ZOOM_TAPS 127
float32_t DMAMEM Fir_Zoom_FFT_Decimate_I1_state[ZOOM_TAPS + 2048 - 1] __attribute__((aligned(4)));
float32_t DMAMEM Fir_Zoom_FFT_Decimate_Q1_state[ZOOM_TAPS + 2048 - 1] __attribute__((aligned(4)));
float32_t DMAMEM Fir_Zoom_FFT_Decimate_I2_state[ZOOM_TAPS + 1024 - 1] __attribute__((aligned(4)));
float32_t DMAMEM Fir_Zoom_FFT_Decimate_Q2_state[ZOOM_TAPS + 1024 - 1] __attribute__((aligned(4)));
float32_t DMAMEM Fir_Zoom_FFT_Decimate1_coeffs[ZOOM_TAPS] __attribute__((aligned(4)));
float32_t DMAMEM Fir_Zoom_FFT_Decimate2_coeffs[ZOOM_TAPS] __attribute__((aligned(4)));

//-------------------------------------------------------------------------------------------------------------
// Code
//-------------------------------------------------------------------------------------------------------------

FLASHMEM void InitHilbertFilters() {
  // Hilbert filters
  // *** TODO: examine these filter coeficients
  // see: https://groups.io/g/SoftwareControlledHamRadio/topic/t41_hilbert_filter_design/113214259
  // ***
  if(t41.DemodMode == DEMOD_FT8) {
    arm_fir_init_f32(&FIR_Hilbert_L, 100, FIR_Hilbert_coeffs_45_FT8, FIR_Hilbert_state_L, 256);
    arm_fir_init_f32(&FIR_Hilbert_R, 100, FIR_Hilbert_coeffs_neg45_FT8, FIR_Hilbert_state_R, 256);
  } else {
    arm_fir_init_f32(&FIR_Hilbert_L, 100, FIR_Hilbert_coeffs_45, FIR_Hilbert_state_L, 256);
    arm_fir_init_f32(&FIR_Hilbert_R, 100, FIR_Hilbert_coeffs_neg45, FIR_Hilbert_state_R, 256);
  }
}

FLASHMEM void InitFIRFilters(int sampleRate) {
  /****************************************************************************************
     Receive decimation and interpolation FIR filters design

      Two-stage decimation and interpolation
        1st stage decimation factor = 4
        2nd stage decimation factor = 2
        desired stopband attenuation = 90
        desired max BW of the filters = 9k
        samplerate before decimation = 192k (original code has 176k which is from Teensy Convolution SDR, why use it here?)
      see Lyons 2011 Two-Stage Decimation chapter 10.2 for the theory (equation 10.3)

      const float32_t n_fpass1 = 9.0 / n_samplerate;
      const float32_t n_fpass2 = 9.0 / (n_samplerate / 4.0);
      const float32_t n_fstop1 = ((n_samplerate / 4.0) - 9.0) / n_samplerate;
      const float32_t n_fstop2 = ((n_samplerate / 8.0) - 9.0) / (n_samplerate / 4.0);

      const uint16_t n_dec1_taps = (1 + (uint16_t)(90.0 / (22.0 * (n_fstop1 - n_fpass1))));
      const uint16_t n_dec2_taps = (1 + (uint16_t)(90.0 / (22.0 * (n_fstop2 - n_fpass2))));

      gives: n_dec1_taps = 27, n_dec2_taps = 33 (see decFilterTaps.xlsx in D:\Projects\Radio\Projects\T41_v12)
  ****************************************************************************************/
  // Decimation filter 1, M1 = 4
  CalcFIRCoeffs(FIR_dec1_coeffs, 27, 9000.0, 90.0, 0, 0.0, sampleRate);
  arm_fir_decimate_init_f32(&FIR_dec1_I, 27, 4.0, FIR_dec1_coeffs, FIR_dec1_I_state, 2048);
  arm_fir_decimate_init_f32(&FIR_dec1_Q, 27, 4.0, FIR_dec1_coeffs, FIR_dec1_Q_state, 2048);

  // Decimation filter 2, M2 = 2
  CalcFIRCoeffs(FIR_dec2_coeffs, 33, 9000.0, 90.0, 0, 0.0, sampleRate / 4.0);
  arm_fir_decimate_init_f32(&FIR_dec2_I, 33, 2, FIR_dec2_coeffs, FIR_dec2_I_state, 512);
  arm_fir_decimate_init_f32(&FIR_dec2_Q, 33, 2, FIR_dec2_coeffs, FIR_dec2_Q_state, 512);

  // Decimation filter 3, M2 = 4 (from 48kps to 12kps)
  CalcFIRCoeffs(FIR_dec3_coeffs, 50, 4000.0, 90.0, 0, 0.0, sampleRate / 4.0);
  arm_fir_decimate_init_f32(&FIR_dec3, 50, 4, FIR_dec3_coeffs, FIR_dec3_state, 512);

  // Interpolation filter 1, L1 = 2
  CalcFIRCoeffs(FIR_int1_coeffs, 48, 9000.0, 90.0, 0, 0.0, sampleRate / 4.0);
  arm_fir_interpolate_init_f32(&FIR_int1_I, 2, 48, FIR_int1_coeffs, FIR_int1_I_state, 256);
  arm_fir_interpolate_init_f32(&FIR_int1_Q, 2, 48, FIR_int1_coeffs, FIR_int1_Q_state, 256);

  // Interpolation filter 2, L2 = 4
  CalcFIRCoeffs(FIR_int2_coeffs, 32, 9000.0, 90.0, 0, 0.0, sampleRate);
  arm_fir_interpolate_init_f32(&FIR_int2_I, 4.0, 32, FIR_int2_coeffs, FIR_int2_I_state, 512);
  arm_fir_interpolate_init_f32(&FIR_int2_Q, 4.0, 32, FIR_int2_coeffs, FIR_int2_Q_state, 512);

  // excite decimate/interpolate filter initialization
  arm_fir_decimate_init_f32(&FIR_dec1_EX_I, 48, 4, coeffs192K_10K_LPF_FIR, FIR_dec1_EX_I_state, 2048); // 192k -> 48k
  arm_fir_decimate_init_f32(&FIR_dec1_EX_Q, 48, 4, coeffs192K_10K_LPF_FIR, FIR_dec1_EX_Q_state, 2048);
  arm_fir_decimate_init_f32(&FIR_dec2_EX_I, 48, 2, coeffs48K_8K_LPF_FIR, FIR_dec2_EX_I_state, 512); // 48k -> 24k
  arm_fir_decimate_init_f32(&FIR_dec2_EX_Q, 48, 2, coeffs48K_8K_LPF_FIR, FIR_dec2_EX_Q_state, 512);
  //arm_fir_decimate_init_f32(&FIR_dec3_EX_I, 48, 2, coeffs_0_8_LPF_FIR, FIR_dec3_EX_I_state, 256); // 24k -> 12k
  //arm_fir_decimate_init_f32(&FIR_dec3_EX_Q, 48, 2, coeffs_0_8_LPF_FIR, FIR_dec3_EX_Q_state, 256);
  arm_fir_decimate_init_f32(&FIR_dec3_EX_I, 48, 2, coeffs12K_8K_LPF_FIR, FIR_dec3_EX_I_state, 256); // v66-9 coef
  arm_fir_decimate_init_f32(&FIR_dec3_EX_Q, 48, 2, coeffs12K_8K_LPF_FIR, FIR_dec3_EX_Q_state, 256);

  arm_fir_interpolate_init_f32(&FIR_int1_EX_I, 2, 48, coeffs48K_8K_LPF_FIR, FIR_int1_EX_I_state, 256); // 24k -> 48k
  arm_fir_interpolate_init_f32(&FIR_int1_EX_Q, 2, 48, coeffs48K_8K_LPF_FIR, FIR_int1_EX_Q_state, 256);
  arm_fir_interpolate_init_f32(&FIR_int2_EX_I, 4, 48, coeffs192K_10K_LPF_FIR, FIR_int2_EX_I_state, 512); // 48k -> 192k
  arm_fir_interpolate_init_f32(&FIR_int2_EX_Q, 4, 48, coeffs192K_10K_LPF_FIR, FIR_int2_EX_Q_state, 512);
  //arm_fir_interpolate_init_f32(&FIR_int3_EX_I, 2, 48, coeffs_0_8_LPF_FIR, FIR_int3_EX_I_state, 128); // 12k -> 24k
  //arm_fir_interpolate_init_f32(&FIR_int3_EX_Q, 2, 48, coeffs_0_8_LPF_FIR, FIR_int3_EX_Q_state, 128);
  arm_fir_interpolate_init_f32(&FIR_int3_EX_I, 2, 48, FIR_int3_12ksps_48tap_2k7, FIR_int3_EX_I_state, 128); // v66-9 coef
  arm_fir_interpolate_init_f32(&FIR_int3_EX_Q, 2, 48, FIR_int3_12ksps_48tap_2k7, FIR_int3_EX_Q_state, 128); //

  // CW filters
  arm_fir_init_f32(&FIR_CW_DecodeL, 64, CW_Filter_Coeffs2, FIR_CW_DecodeL_state, 256);
  arm_fir_init_f32(&FIR_CW_DecodeR, 64, CW_Filter_Coeffs2, FIR_CW_DecodeR_state, 256);

  // init Hilbert filters
  InitHilbertFilters();

  // set audio, decimate and interpolate filters based on current band filter cutoffs
  SetupDemodFilterBW();
}

// Calculate sinc function
float MSinc(int m, float fc) {
  float x = m * HALF_PI;
  if(m == 0)
    return 1.0f;
  else
    return sinf(x * fc) / (fc * x);
}

// calculate zero order modified bessel function of the first kind
float32_t Izero(float32_t x) {
  float32_t x2          = x / 2.0;
  float32_t summe       = 1.0;
  float32_t ds          = 1.0;
  float32_t di          = 1.0;
  float32_t errorlimit  = 1e-9;
  float32_t tmp;

  do
  {
    tmp = x2 / di;
    tmp *= tmp;
    ds *= tmp;
    summe += ds;
    di += 1.0;
  } while(ds >= errorlimit * summe);
  return summe;
}

#include <cmath>

/*****
  Purpose: calc_FIR_coeffs
    // pointer to coefficients variable, no. of coefficients to calculate, frequency where it happens, stopband attenuation in dB,
    // filter type, half-filter bandwidth (only for bandpass and notch)

  Parameter list:
    float * coeffs_I
    int numCoeffs
    float32_t fc
    float32_t Astop
    int type
    float dfc
    float Fsamprate
*****/
FLASHMEM void CalcFIRCoeffs(float *coeffs_I, int numCoeffs, float32_t fc, float32_t Astop, int type, float dfc, float Fsamprate) {
  // modified by WMXZ and DD4WH after
  // Wheatley, M. (2011): CuteSDR Technical Manual. www.metronix.com, pages 118 - 120, FIR with Kaiser-Bessel Window
  // assess required number of coefficients by
  //     numCoeffs = (Astop - 8.0) / (2.285 * TWO_PI * normFtrans);
  // selecting high-pass, numCoeffs is forced to an even number for better frequency response

  int nc    = numCoeffs;
  float32_t Beta;
  float32_t izb;
  float fcf = fc;
  float x, w;
  fc        = fc / Fsamprate;
  dfc       = dfc / Fsamprate;

  // calculate Kaiser-Bessel window shape factor beta from stop-band attenuation
  if(Astop < 20.96) {
    Beta = 0.0;
  } else {
    if(Astop >= 50.0) {
      Beta = 0.1102 * (Astop - 8.71);
    } else {
      Beta = 0.5842 * powf((Astop - 20.96), 0.4) + 0.07886 * (Astop - 20.96);
    }
  }
  memset(coeffs_I, 0.0, numCoeffs * sizeof(float));    //zero entire buffer, important for variables from DMAMEM

  izb = Izero(Beta);
  if(type == 0) { // low pass filter
    fcf = fc * 2.0;
    nc  =  numCoeffs;
  } else if(type == 1) { // high-pass filter
    fcf = -fc;
    nc  =  2 * (numCoeffs / 2);
  } else if((type == 2) || (type == 3)) { // band-pass filter
    fcf = dfc;
    nc  =  2 * (numCoeffs / 2); // maybe not needed
  } else if(type == 4) { // Hilbert transform
    nc  =  2 * (numCoeffs / 2);
    // clear coefficients
    for(int ii = 0; ii < 2 * (nc - 1); ii++) {
      coeffs_I[ii] = 0;
    }
    coeffs_I[nc] = 1;                                   // set real delay
    for(int ii = 1; ii < (nc + 1); ii += 2) {          // set imaginary Hilbert coefficients
      if(2 * ii == nc)
        continue;
      x = (float)(2 * ii - nc) / (float)nc;
      w = Izero(Beta * sqrtf(1.0f - x * x)) / izb; // Kaiser window
      coeffs_I[2 * ii + 1] = 1.0f / (HALF_PI * (float)(ii - nc / 2)) * w ;
    }
    return;
  }

  for(int ii = - nc, jj = 0; ii < nc; ii += 2, jj++) {
    x = (float)ii / (float)nc;
    w = Izero(Beta * sqrtf(1.0f - x * x)) / izb; // Kaiser window
    coeffs_I[jj] = fcf * MSinc(ii, fcf) * w;
  }

  if(type == 1) {
    coeffs_I[nc / 2] += 1;
  } else if(type == 2) {
    for(int jj = 0; jj < nc + 1; jj++)
      coeffs_I[jj] *= 2.0f * cosf(HALF_PI * (2 * jj - nc) * fc);
  } else if(type == 3) {
    for(int jj = 0; jj < nc + 1; jj++)
      coeffs_I[jj] *= -2.0f * cosf(HALF_PI * (2 * jj - nc) * fc);
    coeffs_I[nc / 2] += 1;
  }

}

//////////////////////////////////////////////////////////////////////
// Call to setup filter parameters
// sampleRate in Hz
// fLowcut is low cutoff frequency of filter in Hz
// fHicut is high cutoff frequency of filter in Hz
// Offset is the CW tone offset frequency
// cutoff frequencies range from -sampleRate/2 to +sampleRate/2
//  HiCut must be greater than LowCut
//    example to make 2700Hz USB filter:
//  SetupParameters( 100, 2800, 0, 48000);
//////////////////////////////////////////////////////////////////////

/*****
  Purpose: Calculate audio FFT FIR filter coefficients

  Parameter list:
    float *coeffs_I
    float *coeffs_Q
    int numCoeffs
    float32_t fLoCut
    float32_t fHiCut
    float sampleRate
*****/
FLASHMEM void CalcCplxFIRCoeffs(float * coeffs_I, float * coeffs_Q, int numCoeffs, float32_t fLoCut, float32_t fHiCut, float sampleRate) {
  //calculate some normalized filter parameters
  float32_t nFL = fLoCut / sampleRate;
  float32_t nFH = fHiCut / sampleRate;
  float32_t nFc = (nFH - nFL) / 2.0; // prototype LP filter cutoff
  float32_t nFs = PI * (nFH + nFL); // 2 PI times required frequency shift (fHiCut+fLoCut)/2
  float32_t fCenter = 0.5 * (float32_t)(numCoeffs - 1); //floating point center index of FIR filter
  float32_t x;
  float32_t z;

  memset(coeffs_I, 0.0, numCoeffs * sizeof(float));    //zero entire buffer, important for variables from DMAMEM
  memset(coeffs_Q, 0.0, numCoeffs * sizeof(float));

  //create LP FIR windowed sinc, sin(x)/x complex LP filter coefficients
  for(int i = 0; i < numCoeffs; i++)  {
    x = (float32_t)i - fCenter;
    if( abs((float)i - fCenter) < 0.01) //deal with odd size filter singularity where sin(0)/0==1
      z = 2.0 * nFc;
    else
      switch(FIR_filter_window) {
        case 1:    // 4-term Blackman-Harris --> this is what Power SDR uses
          z = (float32_t)sinf(TWO_PI * x * nFc) / (PI * x) *
              (0.35875 - 0.48829 * cosf( (TWO_PI * i) / (numCoeffs - 1) )
               + 0.14128 * cosf( (FOURPI * i) / (numCoeffs - 1) )
               - 0.01168 * cosf( (SIXPI * i) / (numCoeffs - 1) ) );
          break;

        case 2: // sine
          z = (float32_t)sinf(TWO_PI * x * nFc) / (PI * x) *
              (0.355768 - 0.487396 * cosf( (TWO_PI * i) / (numCoeffs - 1) )
               + 0.144232 * cosf( (FOURPI * i) / (numCoeffs - 1) )
               - 0.012604 * cosf( (SIXPI * i) / (numCoeffs - 1) ) );
          break;

        case 3: // cosine
          z = (float32_t)sinf(TWO_PI * x * nFc) / (PI * x) *
              cosf((PI * (float32_t)i) / (numCoeffs - 1));
          break;

        case 4: // Hann
          z = (float32_t)sinf(TWO_PI * x * nFc) / (PI * x) *
              0.5 * (float32_t)(1.0 - (cosf(PI * 2 * (float32_t)i / (float32_t)(numCoeffs - 1))));
          break;
        default: // Blackman-Nuttall window
          z = (float32_t)sinf(TWO_PI * x * nFc) / (PI * x) *
              (0.3635819
               - 0.4891775 * cosf( (TWO_PI * i) / (numCoeffs - 1) )
               + 0.1365995 * cosf( (FOURPI * i) / (numCoeffs - 1) )
               - 0.0106411 * cosf( (SIXPI * i) / (numCoeffs - 1) ) );
          break;
      }
    //shift lowpass filter coefficients in frequency by (hicut+lowcut)/2 to form bandpass filter anywhere in range
    coeffs_I[i]   = z * cosf(nFs * x);
    coeffs_Q[i]   = z * sinf(nFs * x);
  }
}

/*
  Calculate zoom FIR LPF coefficients using a Kaisered windowed Sinc impulse function with

  Sinc function https://en.wikipedia.org/wiki/Sinc_function
    filter design: https://www.analog.com/media/en/technical-documentation/dsp-book/dsp_book_Ch16.pdf
                   (part of https://www.dspguide.com/ by Steven W. Smith)

  Kaiser window using zero-order modified Bessel: https://en.wikipedia.org/wiki/Kaiser_window
    gives high stopband attenuation w/ low passband ripple

  Kaiser Beta: https://www.researchgate.net/publication/283709407_The_Io-sinh_function_calculation_of_Kaiser_windows_and_design_of_FIR_filters, pg 3, eqn 5
    or https://www-elec.inaoep.mx/~jmram/pds09/oppenheim.pdf pg 474, eqn 7.62
    passband ripple: 10^(-Astop/20)

  Bessel function: https://en.wikipedia.org/wiki/Bessel_function

 * @brief Generates standard Kaiser-Bessel Windowed LPF coefficients for arm_fir_decimate_f32.
 * @param coeffs       Pointer to target float array (Size must equal 'numTaps')
 * @param numTaps      Length of the filter (Should be an ODD number, e.g., 63, 127)
 * @param fc           Desired LPF cutoff frequency in real Hz (from the matrix above)
 * @param Astop        Stopband attenuation in dB (e.g., 70.0f to 80.0f)
 * @param sampleRate   Base operational sample rate (192000.0f)
*/
void CalcZoomFIRCoeffs(float *coeffs, int numTaps, float fc, float Astop, float sampleRate) {
  float fcn = fc / sampleRate; // normalized cutoff frequency
  int N = numTaps - 1;
  float sum = 0.0f;

  // Kaiser window beta
  float beta;
  if(Astop < 21.0f) {
    beta = 0.0f;
  } else if(Astop >= 50.0f) {
    beta = 0.1102f * (Astop - 8.7f);
  } else {
    beta = 0.5842f * powf((Astop - 21.0f), 0.4f) + 0.07886f * (Astop - 21.0f);
  }

  // calculate FIR filter coefficients
  for(int n = 0; n < numTaps; n++) {
    // Sinc
    float sinc;
    float x = fcn * (n - N / 2.0f); // time shifted index
    if(n == N / 2) {
      sinc = 1.0; // x = 0
    } else {
      sinc = sinf(PI * x) / (PI * x);
    }

    // Kaiser-Bessel window
    float y = 2.0f * n / N - 1.0f;
    float w = std::cyl_bessel_i(0.0, beta * sqrtf(1.0f - (y * y))) / std::cyl_bessel_i(0.0, beta);
    //float w = fcn; // sinc impulse function

    // filter coefficients
    coeffs[n] = w * sinc;
    sum += coeffs[n];
  }

  // normalize coefficients for unity gain (0 dB at DC baseline)
  for(int n = 0; n < numTaps; n++) {
    coeffs[n] /= sum;
  }
}

/*
examine required taps:

Using Kaiser's exact formula for 90 dB attenuation (\(A = 90\)), the required tap count for any stage is:\(\text{numTaps}\approx \frac{90-7.95}{14.36\cdot \Delta f}+1=\frac{5.7138}{\Delta f}+1\)(where \(\Delta f\) is the normalized transition width relative to that stage's input sample rate).
*/
void ZoomFilterUpdate(int sampleRate) {
  //int factor1 = t41.SpectrumZoom < 3 ? 2 : (1 << t41.SpectrumZoom) / 2;
  int zoom[5] = {1, 2, 2, 4, 4};
  int factor1 = zoom[t41.SpectrumZoom];
  int factor2 = (1 << t41.SpectrumZoom) / factor1;
  float32_t fc1 = sampleRate / factor1;
  float32_t fc2 = fc1 / factor2;
  //int taps1[5] = {31, 31, 31, 63, 27}; // for 60dB stop band attenuation
  //int taps2[5] = {31, 31, 31, 31, 31};
  //int taps1[5] = {31, 31, 31, 63, 43}; // for 90dB stop band attenuation
  //int taps2[5] = {31, 31, 31, 31, 51};

  //int taps1[5] = {127, 127, 127, 127, 127}; // wide signal spectrum
  //int taps2[5] = {127, 127, 127, 127, 127};
  //int taps1[5] = {63, 63, 63, 63, 63};
  //int taps2[5] = {63, 63, 63, 63, 63};
  int taps1[5] = {47, 47, 47, 47, 47};
  int taps2[5] = {47, 47, 47, 47, 47};
  //int taps1[5] = {31, 31, 31, 31, 31};
  //int taps2[5] = {31, 31, 31, 31, 31};
  //int taps1[5] = {13, 13, 13, 13, 13}; // for dB stop band attenuation
  //int taps2[5] = {13, 13, 13, 13, 13};
  //int taps1[5] = {12, 12, 12, 12, 12}; // for dB stop band attenuation
  //int taps2[5] = {12, 12, 12, 12, 12};
  //float astop = 60.0;
  float astop = 90.0;

  Serial.printf("fc1: %d\tfc2: %d\n", (int)fc1, (int)fc2);

  // frequency spectrum zoom decimation filters
  // estimate number of filter taps required based on filter specs
  // m_NumTaps = (Astop - 8.0) / (2.285*K_2PI*(normFstop - normFpass) ) + 1;
  // (60-8) / (2.285*2*3.14*(*2)) + 1 =
  // 1st decimation stage
  CalcZoomFIRCoeffs(Fir_Zoom_FFT_Decimate1_coeffs, taps1[t41.SpectrumZoom], fc1, astop, (float32_t)sampleRate);

  // 2nd decimation stage
  CalcZoomFIRCoeffs(Fir_Zoom_FFT_Decimate2_coeffs, taps2[t41.SpectrumZoom], fc2, astop, (float32_t)sampleRate / factor1);
}

/*****
  Purpose: change IIR and decimation filters for altered frequency spectrum badwidth.
*****/
FLASHMEM void ZoomFFTFilterUpdate(int sampleRate) {
  float32_t Fstop_Zoom = 0.5 * sampleRate / (1 << t41.SpectrumZoom);
  int factor1 = t41.SpectrumZoom < 3 ? 2 : (1 << t41.SpectrumZoom) / 2;

  Serial.printf("fc1: %d\tfc2: %d\n", (int)Fstop_Zoom, (int)Fstop_Zoom);

  // 1st decimation stage
  CalcFIRCoeffs(Fir_Zoom_FFT_Decimate1_coeffs, 12, Fstop_Zoom, 60, 0, 0.0, (float32_t)sampleRate);

  // 2nd decimation stage
  CalcFIRCoeffs(Fir_Zoom_FFT_Decimate2_coeffs, 12, Fstop_Zoom, 60, 0, 0.0, (float32_t)sampleRate / factor1);
}

FLASHMEM void InitZoomFFTFilter(int sampleRate, uint32_t blockSize /* = 2048 */) {
#if USE_NEW_FIR
  int zoom[5] = {1, 2, 2, 4, 4};
  int factor1 = zoom[t41.SpectrumZoom];
  int factor2 = (1 << t41.SpectrumZoom) / factor1;
  //int taps1[5] = {31, 31, 31, 63, 27}; // for 60dB stop band attenuation
  //int taps2[5] = {31, 31, 31, 31, 31};
  //int taps1[5] = {31, 31, 31, 63, 43}; // for 90dB stop band attenuation
  //int taps2[5] = {31, 31, 31, 31, 51};

  //int taps1[5] = {127, 127, 127, 127, 127};
  //int taps2[5] = {127, 127, 127, 127, 127};
  //int taps1[5] = {63, 63, 63, 63, 63};
  //int taps2[5] = {63, 63, 63, 63, 63};
  //int taps1[5] = {63, 63, 63, 63, 63};
  //int taps2[5] = {63, 63, 63, 63, 63};
  int taps1[5] = {47, 47, 47, 47, 47};
  int taps2[5] = {47, 47, 47, 47, 47};
  //int taps1[5] = {31, 31, 31, 31, 31};
  //int taps2[5] = {31, 31, 31, 31, 31};
  //int taps1[5] = {13, 13, 13, 13, 13};
  //int taps2[5] = {13, 13, 13, 13, 13};

  // *** for testing alternate filter coefficients ***
  ZoomFilterUpdate(sampleRate);

  // 1st decimation stage
  arm_fir_decimate_init_f32(&Fir_Zoom_FFT_Decimate_I1, taps1[t41.SpectrumZoom], factor1, Fir_Zoom_FFT_Decimate1_coeffs, Fir_Zoom_FFT_Decimate_I1_state, blockSize);
  arm_fir_decimate_init_f32(&Fir_Zoom_FFT_Decimate_Q1, taps1[t41.SpectrumZoom], factor1, Fir_Zoom_FFT_Decimate1_coeffs, Fir_Zoom_FFT_Decimate_Q1_state, blockSize);

  // 2nd decimation stage
  arm_fir_decimate_init_f32(&Fir_Zoom_FFT_Decimate_I2, taps2[t41.SpectrumZoom], factor2, Fir_Zoom_FFT_Decimate2_coeffs, Fir_Zoom_FFT_Decimate_I2_state, blockSize / factor1);
  arm_fir_decimate_init_f32(&Fir_Zoom_FFT_Decimate_Q2, taps2[t41.SpectrumZoom], factor2, Fir_Zoom_FFT_Decimate2_coeffs, Fir_Zoom_FFT_Decimate_Q2_state, blockSize / factor1);
#else
  int factor1 = t41.SpectrumZoom < 3 ? 2 : (1 << t41.SpectrumZoom) / 2;
  int factor2 = t41.SpectrumZoom < 2 ? 1 : 2;

  ZoomFFTFilterUpdate(sampleRate);

  // 1st decimation stage
  arm_fir_decimate_init_f32(&Fir_Zoom_FFT_Decimate_I1, ZOOM_TAPS_1, factor1, Fir_Zoom_FFT_Decimate1_coeffs, Fir_Zoom_FFT_Decimate_I1_state, blockSize);
  arm_fir_decimate_init_f32(&Fir_Zoom_FFT_Decimate_Q1, ZOOM_TAPS_1, factor1, Fir_Zoom_FFT_Decimate1_coeffs, Fir_Zoom_FFT_Decimate_Q1_state, blockSize);

  // 2nd decimation stage
  arm_fir_decimate_init_f32(&Fir_Zoom_FFT_Decimate_I2, ZOOM_TAPS_2, factor2, Fir_Zoom_FFT_Decimate2_coeffs, Fir_Zoom_FFT_Decimate_I2_state, blockSize / factor1);
  arm_fir_decimate_init_f32(&Fir_Zoom_FFT_Decimate_Q2, ZOOM_TAPS_2, factor2, Fir_Zoom_FFT_Decimate2_coeffs, Fir_Zoom_FFT_Decimate_Q2_state, blockSize / factor1);
#endif

  Serial.print("i\tc1\tc2\n");
  if(ZOOM_TAPS_1 == ZOOM_TAPS_2) {
    for(int i = 0; i < ZOOM_TAPS_1; i++) {
      Serial.printf("%d\t%d\t%d\n", i, (int)(Fir_Zoom_FFT_Decimate1_coeffs[i]*1000000.0), (int)(Fir_Zoom_FFT_Decimate2_coeffs[i]*1000000.0));
    }
  }
  Serial.println();
}
