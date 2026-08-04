
#include "SDT.h"

#include "AudioConfig.h"
#include "ButtonProc.h"
#include "calibrate.h"
#include "CW_Excite.h"
#include "CWProcessing.h"
#include "Demod.h"
#include "Display.h"
#include "DSP_Fn.h"
#include "Encoders.h"
#include "Exciter.h"
#include "Filter.h"
#include "FIR.h"
#include "ft8.h"
#include "hardware.h"
#include "keyer.h"
#include "Menu.h"
#include "MenuProc.h"
#include "mouse.h"
#include "Noise.h"
#include "Process.h"
#include "psk31.h"
#include "Tune.h"
#include "Utility.h"

#include "debug.h"

#include "catControl.h"
#include "USBManager.h"
#include "connectManager.h"

//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

extern ConnectManager connectManager;
extern CatControl catControl;
extern CatControl wsjtControl;

// *** TODO: this is display dependent, but also fundamental to much of how the DSP process works ***
#define SPECTRUM_RES          512

bool FFTupdated;

arm_biquad_casd_df1_inst_f32 biquad_lowpass1;
float32_t biquad_lowpass1_state[4];
float32_t biquad_lowpass1_coeffs[5] = { 0, 0, 0, 0, 0 };

float32_t audioMaxSquaredAve = 0.01; // this will blow up dBm if 0

// v49.2k note: This can't be DMAMEM.  It will break the S-Meter.
// this was probably because it needed to be aligned, but below works fine for me
// (4k more to heap) perhaps because it's aligned by default in my version
// *** TODO: test need for alignment and incorporate if needed ***
// *** TODO: this is inefficient as it's only used to pass data to the audio spectrum
//           draw routine and only a small portion is used.  Rework to eliminate waste.
float32_t DMAMEM audioSpectBuffer[1024];

uint8_t NB_on = 0; // noise blanker: 0 - off, 1 - on

char atom, currentAtom;
uint8_t ANR_notchOn = 0;

float32_t DMAMEM audioFFT[1024] __attribute__((aligned(4)));
float32_t DMAMEM audioIFFT[1024 + 1];
float32_t DMAMEM prevAudioBuffer_L[256];
float32_t DMAMEM prevAudioBuffer_R[256];

#define USE_LOG10FAST

float32_t DMAMEM freqSpecBuf[SPECTRUM_RES] __attribute__((aligned(4)));

extern arm_fir_decimate_instance_f32 Fir_Zoom_FFT_Decimate_I1, Fir_Zoom_FFT_Decimate_Q1, Fir_Zoom_FFT_Decimate_I2, Fir_Zoom_FFT_Decimate_Q2;

extern int zoomDecFactors[];

//-------------------------------------------------------------------------------------------------------------
// Forwards
//-------------------------------------------------------------------------------------------------------------

void PrepareExciterIQDataCal(int mode);

float VolumeToAmplification(int volume);
void FreqShift1(int blockSize);
void FreqShift2();
bool CalcFreqSpecBuffered(uint32_t blockSize);
void CalcFreqSpec(float32_t* ptrI, float32_t* ptrQ, float multiplier = 1.0);

//-------------------------------------------------------------------------------------------------------------
// Code
//-------------------------------------------------------------------------------------------------------------

FLASHMEM void InitFFTArrays() {
  CLEAR_VAR(audioFFT);
  CLEAR_VAR(prevAudioBuffer_L);
  CLEAR_VAR(prevAudioBuffer_R);
}

FLASHMEM void InitAMDemodBiquadFilter(int sampleRate) {
  // *** TODO: seems this should be calc on filter BW change, but isn't ***
/*
  int LP_F_help = bands[t41.ActiveBand].fHiCut;

  if(LP_F_help < -bands[t41.ActiveBand].fLoCut) {
    LP_F_help = -bands[t41.ActiveBand].fLoCut;
  }

  SetIIRCoeffs(biquad_lowpass1_coeffs, (float32_t)LP_F_help, 1.3, sampleRate / 8.0, 0);  // 1st stage
*/
  SetIIRCoeffs(biquad_lowpass1_coeffs, t41.FilterHiCut, 1.3, sampleRate / 8.0, 0);  // 1st stage

  biquad_lowpass1.numStages = 1;  // set number of stages
  biquad_lowpass1.pCoeffs = biquad_lowpass1_coeffs;      // set pointer to coefficients file

  for(int i = 0; i < 4; i++) {
    biquad_lowpass1_state[i] = 0.0;  // set state variables to zero
  }

  biquad_lowpass1.pState = biquad_lowpass1_state;  // set pointer to the state variables
}

void CalcAudioMax() {
  const arm_cfft_instance_f32* S = &arm_cfft_sR_f32_len512;
  float32_t audioMaxSquared;
  uint32_t audioMaxIndex;
  // *** TODO: 10k here, would be better to just allocated it when in NFM mode! ***
  static float32_t audioNFM[1024], audioNFMi[1024], prevNFMAudioBuffer_L[256], prevNFMAudioBuffer_R[256];
  float32_t audioNFMBuffer[1024];

  // Prepare the audio signal buffers
  // fill buffer with last events audio samples
  for(int i = 0; i < 256; i++) {
    audioNFM[i * 2] = prevNFMAudioBuffer_L[i]; // real
    audioNFM[i * 2 + 1] = prevNFMAudioBuffer_R[i]; // imaginary
  }

  for(int i = 0; i < 256; i++) {
    // copy recent samples to last_sample_buffer for next time
    prevNFMAudioBuffer_L[i] = audioBufferL[i];
    prevNFMAudioBuffer_R[i] = audioBufferR[i];

    // fill recent audio samples into audioFFT (left channel: re, right channel: im)
    audioNFM[512 + i * 2] = audioBufferL[i]; // real
    audioNFM[512 + i * 2 + 1] = audioBufferR[i]; // imaginary
  }

  // transform audio signal to the frequency domain
  arm_cfft_f32(S, audioNFM, 0, 1);

  // apply high and low audio cutoffs with FFT filter mask
  // filter mask calculated in setup of filter mask coefficients: audioFIRFilterMask[]
  arm_cmplx_mult_cmplx_f32(audioNFM, audioFIRFilterMask, audioNFMi, 512);

  for(int k = 0; k < 1024; k++) {
    audioNFMBuffer[1023 - k] = (audioNFMi[k] * audioNFMi[k]);
  }

  arm_max_f32(audioNFMBuffer, 1024, &audioMaxSquared, &audioMaxIndex);  // Max value of squared bin magnitued in audio
  audioMaxSquaredAve = .5 * audioMaxSquared + .5 * audioMaxSquaredAve;  // running averaged values
}

// imComp: FFT has an imaginary component (default: true)
// reset:  reset FFT
void AudioDSP(bool updateSpectrumData, bool imComp = true) {
  const arm_cfft_instance_f32* S = &arm_cfft_sR_f32_len512;
  float32_t audioMaxSquared;
  uint32_t audioMaxIndex;

  // Prepare the audio signal buffers
  // fill buffer with last events audio samples
  for(int i = 0; i < 256; i++) {
    audioFFT[i * 2] = prevAudioBuffer_L[i]; // real
    audioFFT[i * 2 + 1] = imComp ? prevAudioBuffer_R[i] : 0.0; // imaginary
  }

  for(int i = 0; i < 256; i++) {
    // copy recent samples to last_sample_buffer for next time
    prevAudioBuffer_L[i] = audioBufferL[i];
    if(imComp) prevAudioBuffer_R[i] = audioBufferR[i];

    // fill recent audio samples into audioFFT (left channel: re, right channel: im)
    audioFFT[512 + i * 2] = audioBufferL[i]; // real
    audioFFT[512 + i * 2 + 1] = imComp ? audioBufferR[i] : 0.0; // imaginary
  }

  // transform audio signal to the frequency domain
  arm_cfft_f32(S, audioFFT, 0, 1);

  // apply high and low audio cutoffs with FFT filter mask
  // filter mask calculated in setup of filter mask coefficients: audioFIRFilterMask[]
  arm_cmplx_mult_cmplx_f32(audioFFT, audioFIRFilterMask, audioIFFT, 512);

  // process audio frequency spectrum if requested
  if(updateSpectrumData) {
    for(int k = 0; k < 1024; k++) {
      audioSpectBuffer[1023 - k] = (audioIFFT[k] * audioIFFT[k]);
    }

    if(t41.DemodMode != DEMOD_NFM)
    {
      arm_max_f32(audioSpectBuffer, 1024, &audioMaxSquared, &audioMaxIndex);  // Max value of squared bin magnitued in audio
      audioMaxSquaredAve = .5 * audioMaxSquared + .5 * audioMaxSquaredAve;  // running averaged values
    }

    // *** TODO: this is from v12 - reconcile calibration calls within Process.cpp ***
    // this was added May 5, 2025 when adding frequency calibration
    // *** TODO: why??? ***
    //if(calibrateItem < 0) {
    //  DrawSmeterBar();
    //}
  }

  // transform audio signal back to the time domain
  arm_cfft_f32(S, audioIFFT, 1, 1);
}

/*****
  Purpose: Read audio from Teensy Audio Library
           Calculate FFT for display
           Process audio into SSB signal
           Output audio to amplifier

   Parameter List:
      bool updateSpectrumData: true: prepares frequency and audio spectrum data for display
                               false (default): skips these calculations

   Return value:
      0: false: not enough data to process
      1: input stream was processed
      2: spectrums updates

    *** Call only when the required number of blocks are available or use CheckReceiverData,
        an inline function that check for this condition.  This eliminate the function call
        overhead. This prevents churn given the frequency of checking vs success (75 to 1). ***
*****/
int ProcessReceiverData(bool updateSpectrumData /* = false */) {
  static float32_t audiotmp = 0.0f;
  float32_t w;
  static float32_t wold = 0.0f;
  float rfGainValue, intScaler;
  // *** the amount of data required by the frequency spectrum calc depends on the zoom factor ***
  bool freqSpecUpdatedThisLoop = false; // true: spectrums updated, otherwise false
  bool anotherPassRequired = false; // will be true at 8x and 16x zoom
  // audio spectrum calc works with 256 samples which is 2 blocks at 44.1kHz or 16 blocks at 192kHz decimated by 8 or 24Hz
  int blocks = GetBlocks();

  /**********************************************************************************
        Get samples from queue buffers
        Teensy Audio Library stores ADC data in two buffers size=128, Q_in_L and Q_in_R as initiated from the audio lib.
        Then the buffers are  read into two arrays sp_L and sp_R in blocks of 128 up to 2048 bytes.  The arrarys are
        of size BUFFER_SIZE*N_BLOCKS.  BUFFER_SIZE is 128, N_BLOCKS = FFT_L / 2 / BUFFER_SIZE * DF = 16 with DF = 8 and FFT_L = 512
        BUFFER_SIZE*N_BLOCKS = 2048 samples
     **********************************************************************************/
  //
  // The T41 takes ~1.5-5.0 ms (depending on display update, mode and options) to process 16 audio packets
  // afterwards it may take up to 10 ms to refill the buffers until 16 packets are available (thus this if block is
  // skipped and we return immediately to DrawFreqSpectrum to continue updating the display)
  // This entire process serves to regulate the audio output stream and changes may affect that stream.
  // For example, playing a wav file without some display updates (audio spectrum
  // for example), will cause a faster (unnatural) playback speed.
  //
  // A note for future reference:
  // This https://www.pjrc.com/teensy/td_libs_AudioNewObjects.html indicates the library calls an audio object's update function
  // every 128 samples and that the update function is run from a low priority interrupt. We can idle here until the
  // number of packets is sufficient to process.  I wasn't able to use this though with an interval timer driven process
  // even with reenabling interrupts during the idle loop.  Perhaps the low priority of the update interrupt was affecting this.
  //
  // we allow input buffer availability to regulate FT8 wav file decoding otherwise we'll process the wav file too fast

  SETPROFILEPIN(PROFILER_PROCESS_RX);

  elapsedMicros usec = 0;

  // transfer IQ samples to audio buffers and convert them to float (> -1.0 to < 1.0)
  // left buffer represents in-phase, I, sample and right buffer represents quadrature, Q, sample
  for(int i = 0; i < blocks; i++) {
    arm_q15_to_float(Q_in_L.readBuffer(), &audioBufferL[128 * i], 128);
    arm_q15_to_float(Q_in_R.readBuffer(), &audioBufferR[128 * i], 128);

    Q_in_L.freeBuffer();
    Q_in_R.freeBuffer();
  }

  // *** TODO: consider if this is needed for FT8 ***
  // set RF gain for all bands
  rfGainValue = pow(10, (float)t41.RFGain / 20);
  arm_scale_f32(audioBufferL, rfGainValue, audioBufferL, blocks * 128);
  arm_scale_f32(audioBufferR, rfGainValue, audioBufferR, blocks * 128);

  // preprocess signals
  // *** TODO: investigate DC bias removal algorithm and it's application to FT8 ***
  switch(t41.DemodMode) {
    //case DEMOD_FT8:
    //  break;

    default:
      RemoveDCBias();
      break;
  }

  // apply RF gain
  arm_scale_f32(audioBufferL, bands[t41.ActiveBand].rfGain, audioBufferL, blocks * 128);
  arm_scale_f32(audioBufferR, bands[t41.ActiveBand].rfGain, audioBufferR, blocks * 128);

  /**********************************************************************************
    Clear Buffers
    The original T41 code clears the Teensy audio buffers here if there are more than
    25-packets available.  I found this limit restrictive and caused audio atrifacts.
    I deleted the code block.  You can read more about it here:
    https://new.reddit.com/r/T41_EP/comments/1dus4d0/clearing_up_some_artifacts_in_my_t41_audio_stream/
  **********************************************************************************/
  // this is still helpful for troubleshooting at times when the audio process isn't working correctly
  // *** TODO: needed for current state of internal FT8 decoding, DEMOD_FT8_INTERNAL, hangs otherwise, though interrupts work ***
  if((Q_in_L.available() > 50) && (Q_in_R.available() > 50)) {
    //if(sendGet) {
      //Serial.println("clearing @ ProcessReceiverData ...");
    //}
    Q_in_L.clear();
    Q_in_R.clear();
    t41.DroppedBlock = 1;
  }

  // apply IQ amplitude and phase correction for RX op amp differences
  // *** this does not perform sideband selection, that occurs when applying audioFIRFilterMask ***
  // *** TODO: examine whether specific factors are needed for USB/FT8 or if a simple negation is sufficient ***
  ApplyIQCorrectionFactors(audioBufferL, audioBufferR, IQAmpCorrectionFactor[t41.ActiveBand], IQPhaseCorrectionFactor[t41.ActiveBand], blocks * 128);

  /**********************************************************************************
      Perform a 256 point FFT for the spectrum display on the basis of the first 256 complex values
      of the raw IQ input data this saves about 3% of processor power compared to calculating
      the magnitudes and means of the 4096 point FFT for the display

      Only go there from here, if magnification == 1
  ***********************************************************************************************/

  if((t41.SpectrumZoom == 0) && updateSpectrumData) {
    CalcFreqSpec(audioBufferL, audioBufferR);
    freqSpecUpdatedThisLoop = true;

  // *** TODO: this is from v12 - reconcile calibration calls within Process.cpp ***
    //if(calibrateItem == 1) {
    //  FFTupdated = true; // *** TODO: consolidate this as return from ShowSpectrum2 ***
    //  return true; // *** TODO: check that receive calibrate is coded to get the data it needs ***
    //}
  }

  /**********************************************************************************
      Frequency translation by Fs/4 without multiplication from Lyons (2011): chapter 13.1.2 page 646
      together with the savings of not having to shift/rotate the audioFFT, this saves
      about 1% of processor use

      This is for +Fs/4 [moves receive frequency to the left in the spectrum display]
        audioBufferL contains I = real values
        audioBufferR contains Q = imaginary values
        xnew(0) =  xreal(0) + jximag(0)
            leave first value (DC component) as it is!
        xnew(1) =  - ximag(1) + jxreal(1)
  **********************************************************************************/
  FreqShift1(blocks * 128);

  /**********************************************************************************
      Spectrum zoom displays a magnified display of the data around the translated
      receive frequency.  It uses the shifted spectrum, so the center "hump" around DC is
      shifted by Fs/4.  Buffering and processing is done in the CalcFreqSpecBuffered function.
  **********************************************************************************/
  // Kick off frequency spectrum FFT routine only once for each audio process loop
  if(t41.SpectrumZoom != 0 && updateSpectrumData) {
    // flag if frequency spectrum updated or more passes required
    if(CalcFreqSpecBuffered(blocks * 128)) {
      freqSpecUpdatedThisLoop = true;
    } else {
      anotherPassRequired = true;
    }
  }

  YieldToEthernet();

  /*************************************************************************************************
      freq_conv2()

      FREQUENCY CONVERSION USING A SOFTWARE QUADRATURE OSCILLATOR
      Creates a new IF frequency to allow the tuning window to be moved anywhere in the current display.
      THIS VERSION calculates the COS AND SIN WAVE on the fly - uses double precision float

      MAJOR ADVANTAGE: frequency conversion can be done for any frequency !

      large parts of the code taken from the mcHF code by Clint, KA7OEI, thank you!
        see here for more info on quadrature oscillators:
      Wheatley, M. (2011): CuteSDR Technical Manual Ver. 1.01. - http://sourceforge.net/projects/cutesdr/
      Lyons, R.G. (2011): Understanding Digital Processing. – Pearson, 3rd edition.
  *************************************************************************************************/
  FreqShift2();

  if(t41.CalState != NOT_CAL_STATE) return freqSpecUpdatedThisLoop ? 2 : 1;

  YieldToEthernet();

  /**********************************************************************************
      Decimation
      Resample (Decimate) the shifted time signal, first by 4, then by 2.  Each time the
      signal is decimated by an even number, the spectrum is reversed.  Resampling twice
      returns the spectrum to the correct orientation.
      Signal has now been shifted to base band, leaving aliases at higher frequencies,
      which are removed at each decimation step using the Arm combined decimate/filter function.
      If the starting sample rate is 192K SPS,   after the combined decimation, the sample rate is
      now 192K/8 = 24K SPS.  The array size is also reduced by 8, making FFT calculations much faster.
      The effective bandwidth (up to Nyquist frequency) is 12KHz.
  **********************************************************************************/
  switch(t41.DemodMode) {
    case DEMOD_PSK31:
      // decimation-by-4 in-place!
      arm_fir_decimate_f32(&FIR_dec1_I, audioBufferL, audioBufferL, 2048);
      arm_fir_decimate_f32(&FIR_dec1_Q, audioBufferR, audioBufferR, 2048);

      // we're now at 48k samples per second, the rate used by the PSK31 routines
      // transfer this to the PSK31 buffer
      // *** TODO: finish psk31 work ***
      // but for now just continue
      // *** TODO: the below breaks audio processing (audio buffers fill), don't know why ***
      // decimation-by-2 in-place
      //arm_fir_decimate_f32(&FIR_dec2_I, audioBufferL, audioBufferL, 512);
      //arm_fir_decimate_f32(&FIR_dec2_Q, audioBufferR, audioBufferR, 512);
      break;

    case DEMOD_FT8:
      // at 44.1kHz
      break;

    case DEMOD_PSK31_WAV:
      // *** TODO: refactored code needs work ***
      ProcessPSK31WaveData();
      break;

    #ifdef USE_BUFFERED_FT8_WAV
    case DEMOD_FT8_INTERNAL:
      SETPROFILEPIN(PROFILER_RX_TX);
      // transfer to wav buffer to audio buffers
      // and interpolate to 24 kHz to get audio signal for T41
      // *** TODO: evaluate if use of CMISS_DSP library is better ***
        // interpolate to 24 kHz to get audio signal for T41
        // *** TODO: evaluate if use of CMISS_DSP library is better ***
      // *** TODO: this assumes DEMOD_FT8_INTERNAL mode is always preceeded by DEMOD_FT8_WAV
      //    to populate ft8WavBuf ***
      if(ReadBufferedFT8Wav(audioBufferR, 128)) {
        audioBufferL[0] = audioBufferR[0];
        for(unsigned i = 1; i < 128; i++) {
          audioBufferL[2*i-1] = (audioBufferR[i-1] + audioBufferR[i]) / 2;
          audioBufferL[2*i] = audioBufferR[i];
        }
      }
      RESETPROFILEPIN(PROFILER_RX_TX);
      break;
    #endif

    case DEMOD_FT8_WAV:
      // get samples from wav file (assumed open)
      // pull data at the same rate as the T41
      // audio is 256 bytes, 24 kHz at this point
      // wav file sample rate is 12 kHz
      // get a half sized sample and interpolate to the proper size/rate
      // wav FT8 signal data to audioBufferR, audio to audioBufferL
      SETPROFILEPIN(PROFILER_RX_TX);
      if(ReadFT8Wav(audioBufferR, 128)) {
        // interpolate to 24 kHz to get audio signal for T41
        // *** TODO: evaluate if use of CMISS_DSP library is better ***
        audioBufferL[0] = audioBufferR[0];
        for(unsigned i = 1; i < 128; i++) {
          audioBufferL[2*i-1] = (audioBufferR[i-1] + audioBufferR[i]) / 2;
          audioBufferL[2*i] = audioBufferR[i];
        }

        // we're using the audio input buffers to regulate the pace of the output stream
        // without this we'll play the wave file about 3 times faster than normal
        // *** need to check whether we're clipping any of our output with this
        //      not a big priority unless we want this to be a standard feature ***
        // *** this slows things down with live FT8 processing of the wav file ***
        //if((Q_in_L.available() > 50) && (Q_in_R.available() > 50)) {
        //  Serial.println("clearing @ ProcessReceiverData DEMOD_FT8_WAV ...");
        //  Q_in_L.clear();
        //  Q_in_R.clear();
        //}
      }
      RESETPROFILEPIN(PROFILER_RX_TX);
      break;

    default:
      // decimate by 8x for all other modes

      // decimation-by-4 in-place
      arm_fir_decimate_f32(&FIR_dec1_I, audioBufferL, audioBufferL, 2048);
      arm_fir_decimate_f32(&FIR_dec1_Q, audioBufferR, audioBufferR, 2048);

      // decimation-by-2 in-place
      arm_fir_decimate_f32(&FIR_dec2_I, audioBufferL, audioBufferL, 512);
      arm_fir_decimate_f32(&FIR_dec2_Q, audioBufferR, audioBufferR, 512);

      // now at 24kps
      break;
  }

  YieldToEthernet();

  // audio DSP
  // apply audio filter cutoffs and calculate audio spectrum data
  switch(t41.DemodMode) {
    case DEMOD_NFM:
      CalcAudioMax();

      // Prepare the audio signal buffers
      // fill recent audio samples into audioFFT (left channel: re, right channel: im)
      // we'll use this to demodulate the NFM signal
      for(int i = 0; i < 256; i++) {
        audioFFT[512 + i * 2] = audioBufferL[i]; // real
        audioFFT[512 + i * 2 + 1] = audioBufferR[i]; // imaginary
      }
      break;

    //case DEMOD_FT8:
    //  // *** TODO: consider if AGC (in default below) for FT8 is desirable with WSJT-X ***
    //  // *** without AGC the T41 volume is less in this mode than equivalent SSB ***
    //  AudioDSP(freqSpecUpdatedThisLoop);
    //  break;

    #ifdef USE_BUFFERED_FT8_WAV
    case DEMOD_FT8_INTERNAL: // *** TODO: this is USE_BUFFERED_FT8_WAV only ***
    #endif
    case DEMOD_FT8_WAV:
      //AudioDSP(freqSpecUpdatedThisLoop, 20, false); // no imaginary component for these
      AudioDSP(freqSpecUpdatedThisLoop, false); // no imaginary component for these
      break;

    default:
      // prepare audio signals for all other modes
      AudioDSP(freqSpecUpdatedThisLoop);

      // apply automatic gain control
      // AGC acts upon on the audio data in audioIFFT
      // *** TODO: evaluate effectiveness and proper placement of this AGC function ***
      if(t41.CalState == NOT_CAL_STATE) AGC();
      break;
  }

  YieldToEthernet();

  /**********************************************************************************
    Demodulation
      time domain output is a combination of the real part (left channel) AND the imaginary part (right channel) of the second half of the audioFFT
      The demod mode is accomplished by selecting/combining the real and imaginary parts of the output of the IFFT process.
  **********************************************************************************/
  switch(t41.DemodMode) {
    case DEMOD_AM:
      // *** see xamd() for an alternative: https://github.com/TAPR/OpenHPSDR-wdsp/blob/master/wdsp%202.00/Source/amd.c ***
      for(int i = 0; i < 256; i++) {     // Magnitude estimation Lyons (2011): page 652 / libcsdr
        audiotmp = AlphaBetaMag(audioIFFT[512 + (i * 2)], audioIFFT[512 + (i * 2) + 1]);
        // DC removal filter -----------------------
        w = audiotmp + wold * 0.99f; // Response to below 200Hz
        audioBufferL[i] = w - wold;
        wold = w;
      }
      arm_biquad_cascade_df1_f32(&biquad_lowpass1, audioBufferL, audioBufferR, 256);
      arm_copy_f32(audioBufferR, audioBufferL, 256);
      break;

    case DEMOD_NFM:
      // *** TODO: NFM demod sound rough, similar to when buffers aren't prepared properly one loop to next.  Investigate ***

      // three versions to select from:
      //  (1) - fmdemod_quadri_novect_cf
      //  (2) - nfmdemod
      //  (3) - arm_max_f32
      // the first two have about same performance, the third didn't perform well on my test signal

      nfmdemod(&audioFFT[512], audioBufferL, 256);

      arm_scale_f32(audioBufferL, AUDIO_SCALER_NFM, audioBufferL, 256);

      // limit the demodulated signal
      for(int i = 1; i < 256; i++) {
        float32_t tmp = audioBufferL[i];

        // limit it to -1 <= tmp <= 1
        // modified from limit_ff in libcsdr.c from https://github.com/ha7ilm/csdr
        tmp = (1 < tmp) ? 1 : tmp;
        tmp = (-1 > tmp) ? -1 : tmp;
        audioBufferL[i] = tmp;
      }

      // no difference in audio with this
      //arm_biquad_cascade_df1_f32(&biquad_lowpass1, audioBufferL, audioBufferR, 256);
      //arm_copy_f32(audioBufferR, audioBufferL, 256);

      // buzz and muffled sound with this deemphasis filter
      // *** TODO: see: https://sdr.hu/static/bsc-thesis.pdf section 10.6 De-emphasis to investigate problems here ***
      //deemphasis_nfm_ff(audioBufferL, audioBufferR, 256, sampleRate / 8.0);
      //arm_copy_f32(audioBufferR, audioBufferL, 256);

      //deemphasis_nfm_ff(audioBufferL, audioBufferR, 256, sampleRate / 8.0);
      //arm_biquad_cascade_df1_f32(&biquad_lowpass1, audioBufferR, audioBufferL, 256);

      //arm_biquad_cascade_df1_f32(&biquad_lowpass1, audioBufferL, audioBufferR, 256);
      //deemphasis_nfm_ff(audioBufferR, audioBufferL, 256, sampleRate / 8.0);

      // process audio for demodulated NFM and FT8 wave file
      AudioDSP(freqSpecUpdatedThisLoop, false); // no imaginary component for these

      // apply automatic gain control
      // AGC acts upon on the audio data in audioIFFT
      // *** TODO: evaluate effectiveness and proper placement of this AGC function ***
      if(t41.CalState == NOT_CAL_STATE) AGC();
      break;

    case DEMOD_SAM:
      AMDecodeSAM();
      break;

    case DEMOD_PSK31_WAV:
      // determine the second derivative of the phase angle
      //Psk31Decoder(&audioFFT[512], audioBufferL_EX, 256);
      Psk31PhaseShiftDetector(&audioFFT[512], audioBufferL_EX, 256);
      break;

    default:
      break;
  }

  // additional DSP work #1
  switch(t41.DemodMode) {
    // no additional work
    case DEMOD_AM:
    case DEMOD_SAM:
    case DEMOD_PSK31:
    case DEMOD_PSK31_WAV:
      break;

    //case DEMOD_FT8_WAV: // *** TODO: should wav data be passed through audio filters ***
    default:
      // transfer audio signal back to buffer
      for(int i = 0; i < 256; i++) {
        audioBufferL[i] = audioIFFT[512 + (i * 2)];
      }
      break;
  }

  // additional DSP work #2
  switch(t41.DemodMode) {
    case DEMOD_FT8_INTERNAL:
      #ifndef USE_BUFFERED_FT8_WAV
      // prepare FT8 library signal
      // interpolation by 2 (24kps to 48kps)
      arm_fir_interpolate_f32(&FIR_int1_I, audioBufferL, audioBufferR, 256);

      // decimation by 4 to (48kps to 12kps)
      arm_fir_decimate_f32(&FIR_dec3, audioBufferR, audioBufferR, 512);
      #endif

      // fall through

    case DEMOD_FT8_WAV:
      // transfer 12kHz data to ft8_lib
      BufferFT8Data(audioBufferR, 128);
      break;

    default:
      break;
  }

  // send audio data to control app if applicable
  //if(freqSpecUpdatedThisLoop && controlDataFlag) {
    //for(int i = 0; i < AUDIO_SPEC_RES; i++) {
    //  // audioYPixel is already >= 0, limit it to 255
    //  specData[i] = (uint8_t)(audioYPixel[i] > 255 ? 255 : audioYPixel[i]);
    //}
    //T41ControlSendData(specData, AUDIO_SPEC_RES);
  //}

#ifdef T41_REMOTE_DISPLAY
  if(connected) {
    for(int i = 0; i < AUDIO_SPEC_RES; i++) {
      // audioYPixel is already >= 0, limit it to 255
      audioData[i] = (uint8_t)(audioYPixel[i] > 255 ? 255 : audioYPixel[i]);
    }
  }
#endif

  // apply receive EQ if set
  if(receiveEQFlag == ON ) {
    DoReceiveEQ();
    //arm_copy_f32(audioBufferL, audioBufferR, 256);
  }

  /**********************************************************************************
    Noise Reduction
    3 algorithms working 3-15-22
    NR_Kim
    Spectral NR
    LMS variable leak NR
  **********************************************************************************/
  switch(t41.NoiseFilter) {
    case 0:                               // NR Off
      break;
    case 1:                               // Kim NR
      Kim1_NR();
      arm_scale_f32(audioBufferL, 30, audioBufferL, 256);
      //arm_scale_f32(audioBufferR, 30, audioBufferR, 256);
      break;
    case 2:                               // Spectral NR
      SpectralNoiseReduction();
      break;
    case 3:                               // LMS NR
      Xanr(0);
      arm_scale_f32(audioBufferL, 1.5, audioBufferL, 256);
      //arm_scale_f32(audioBufferR, 2, audioBufferR, 256);
      break;
  }

  // apply automatic notch if set
  if(ANR_notchOn == 1) {
    Xanr(1);
    arm_copy_f32(audioBufferR, audioBufferL, 256);
  }

  /**********************************************************************************
    EXPERIMENTAL: noise blanker
    by Michael Wild
  **********************************************************************************/
  if(NB_on != 0) {
    NoiseBlanker(audioBufferL, audioBufferR);
    arm_copy_f32(audioBufferR, audioBufferL, 256);
  }

  if(t41.RadioState == RECEIVE_STATE) {
    DoCWReceiveProcessing();

    // ----------------------  CW Narrow band filters -------------------------
    if(t41.CWFilterIndex != 5) {
      switch(t41.CWFilterIndex) {
        case 0:  // 0.8 KHz
          arm_biquad_cascade_df2T_f32(&S1_CW_AudioFilter1, audioBufferL, audioBufferR, 256);
          arm_copy_f32(audioBufferR, audioBufferL, 256);
          break;

        case 1: // 1.0 KHz
          arm_biquad_cascade_df2T_f32(&S1_CW_AudioFilter2, audioBufferL, audioBufferR, 256);
          arm_copy_f32(audioBufferR, audioBufferL, 256);
          break;

        case 2: // 1.3 KHz
          arm_biquad_cascade_df2T_f32(&S1_CW_AudioFilter3, audioBufferL, audioBufferR, 256);
          arm_copy_f32(audioBufferR, audioBufferL, 256);
          break;

        case 3: // 1.8 KHz
          arm_biquad_cascade_df2T_f32(&S1_CW_AudioFilter4, audioBufferL, audioBufferR, 256);
          arm_copy_f32(audioBufferR, audioBufferL, 256);
          break;

        case 4:  // 2.0 KHz
          arm_biquad_cascade_df2T_f32(&S1_CW_AudioFilter5, audioBufferL, audioBufferR, 256);
          arm_copy_f32(audioBufferR, audioBufferL, 256);
          break;

        case 5:  //Off
          break;
      }
    }
  }

  // ======================================Interpolation  ================
  switch(t41.DemodMode) {
    case DEMOD_FT8:
      // not needed, we're at a 44.1kHz sample rate and haven't decimated
      // *** this scaler works for a Windows input sound device volume of 10 to give
      // the recommended 30dB input level on WSJT-X ***
      intScaler = 0.1;
      break;

    default:
      // interpolation-by-2
      arm_fir_interpolate_f32(&FIR_int1_I, audioBufferL, audioIFFT, 256);

      // interpolation-by-4
      arm_fir_interpolate_f32(&FIR_int2_I, audioIFFT, audioBufferL, 512);

      // scale by 8x to compensate for the interpolation
      intScaler = 8.0;
      break;
  }

  /**********************************************************************************
    Digital Volume Control
  **********************************************************************************/
  // v11 has a hardware MUTE output pin that is unrelated to this, at least currently.
  // *** TODO: there is currently no mechanism to set mute. ***
  /*
  int mute = 0; // 0 - normal volume, 1 - mute (*** this is never changed ***)
  if(mute == 1) {
    arm_scale_f32(audioBufferL, 0.0, audioBufferL, blocks * 128);
  } else if(mute == 0) {
    // this includes a factor of 8x to compensate for the interpolation above
    arm_scale_f32(audioBufferL, 8.0 * VolumeToAmplification(t41.AudioVolume) * VOL_FACTOR, audioBufferL, blocks * 128);
  }
  */
  arm_scale_f32(audioBufferL, intScaler * VolumeToAmplification(t41.AudioVolume) * VOL_FACTOR, audioBufferL, blocks * 128);

  /**********************************************************************************
    CONVERT TO INTEGER AND PLAY AUDIO
  **********************************************************************************/
  for(int i = 0; i < blocks; i++) {
    int16_t *buf = Q_out_L.getBuffer();
    if(buf != nullptr) {
      arm_float_to_q15(&audioBufferL[i*128], buf, 128);
        Q_out_L.playBuffer();
    } else {
      //Serial.println("skipped ProcessReceiverData output...");
      t41.DroppedBlock = 1;
    }
  }

  /*
  // volume testing
  float tmp;
  static float max = 0;
  uint32_t index;
  arm_max_f32(audioBufferL, 2048, &tmp, &index);
  if(tmp > max) {
    max = tmp;
  }

  Serial.print(tmp*32768.0);Serial.print(", "); Serial.println(max*32768.0);
  */

  elapsed_micros_sum = elapsed_micros_sum + usec;
  elapsed_micros_idx_t++;

  RESETPROFILEPIN(PROFILER_PROCESS_RX);

  if(anotherPassRequired) return 3;
  else return freqSpecUpdatedThisLoop ? 2 : 1;
}

/*****
  Purpose: scale volume from 0 to 100

  Parameter list:
    int volume        the current reading
*****/
float VolumeToAmplification(int volume) {
  float x = volume / 100.0f;  // range 0 to 100
  float ampl = 5 * x * x;

  return ampl;
}

/*****
  Shift audio signals to baseband

    Frequency translation by Fs/4 without multiplication from Lyons (2011): chapter 13.1.2 page 646
    together with the savings of not having to shift/rotate the audioFFT

    Function set for +Fs/4 shift which shift signals toward baseband
        audioBufferL contains I = real values
        audioBufferR contains Q = imaginary values
        xnew(0) =  xreal(0) + jximag(0)
            leave first value (DC component) as it is!
        xnew(1) =  - ximag(1) + jxreal(1)
           ...

    *** if a freq is shifted beyond the nyquist freq it causes a -freq image ***

    Shift away from baseband:
    Code is for -Fs/4
    // shift down Fs/4
    xreal = audioBufferL[i + 1];
    ximag = audioBufferR[i + 1];
    audioBufferL[i + 1] = ximag; // xnew(1) = ximag(1) - jxreal(1)
    audioBufferR[i + 1] = -xreal;
    xreal = audioBufferL[i + 2];
    ximag = audioBufferR[i + 2];
    audioBufferL[i + 2] = -xreal; // xnew(2) = -xreal(2) - jximag(2)
    audioBufferR[i + 2] = -ximag;
    xreal = audioBufferL[i + 3];
    ximag = audioBufferR[i + 3];
    audioBufferL[i + 3] = -ximag; // xnew(3) = -ximag(3) + jxreal(3)
    audioBufferR[i + 3] = xreal;
*****/
void FreqShift1(int blockSize) {
  float32_t xreal, ximag;

  for(int i = 0; i < blockSize; i += 4) {
    // shift up Fs/4
    xreal = audioBufferL[i + 1];
    ximag = audioBufferR[i + 1];
    audioBufferL[i + 1] = -ximag; // xnew(1) = -ximag(1) + jxreal(1)
    audioBufferR[i + 1] = xreal;
    xreal = audioBufferL[i + 2];
    ximag = audioBufferR[i + 2];
    audioBufferL[i + 2] = -xreal; // xnew(2) = -xreal(2) - jximag(2)
    audioBufferR[i + 2] = -ximag;
    xreal = audioBufferL[i + 3];
    ximag = audioBufferR[i + 3];
    audioBufferL[i + 3] = ximag; // xnew(3) = ximag(3) - jxreal(3)
    audioBufferR[i + 3] = -xreal;
  }

  // these will be used for fine tune adjustment in FreqShift2
  for(int i = 0; i < blockSize; i ++) {
    audioBufferL_EX[i] = audioBufferL[i];
    audioBufferR_EX[i] = audioBufferR[i];
  }
}

/*****
  Shift Receive frequency by an arbitray amount

    Notes:  Routine includes checks to ensure the frequency selection stays within the bounds of the
    displayed spectrum
    Also included a variable frequency step, depending on how fast the encoder id turned.  Step varies from 50Hz/step to 10KHz/step

    freq_conv2()

    FREQUENCY CONVERSION USING A SOFTWARE QUADRATURE OSCILLATOR (NCO)

    THIS VERSION calculates the COS AND SIN WAVE on the fly AND IS SLOW

    MAJOR ADVANTAGE: frequency conversion can be done for any frequency !

    large parts of the code taken from the mcHF code by Clint, KA7OEI, thank you!
      see here for more info on quadrature oscillators:
    Wheatley, M. (2011): CuteSDR Technical Manual Ver. 1.01. - http://sourceforge.net/projects/cutesdr/
    Lyons, R.G. (2011): Understanding Digital Processing. – Pearson, 3rd edition.
    Requires 4 complex multiplies and two adds per data point within the time domain buffer.  Applied after the data
    stream is sent to the Zoom FFT , but befor decimation.
*****/
void FreqShift2() {
  uint i;
  int sideToneShift = 0;
  int CWFreqShift = 750;
  float32_t NCO_INC;
  double OSC_COS;
  double OSC_SIN;
  static double Osc_Vect_Q = 1.0;
  static double Osc_Vect_I = 0.0;
  double Osc_Gain = 0.0;
  double Osc_Q = 0.0;
  double Osc_I = 0.0;

  // *** TODO: why is this here? ***
  //if(fineTuneEncoderMove != 0L) {
  //  if(NCOFreq > 40000L) {
  //    NCOFreq = 40000L;
  //  }
  //
  //  currentFreqA = centerFreq + NCOFreq;
  //}

  if(t41.RadioMode == CW_MODE ) {
    if(t41.DemodMode == 1) {
      sideToneShift = CWFreqShift;
    } else {
      if(t41.DemodMode == 0) {
        sideToneShift = -CWFreqShift;
      }
    }
  }

  NCO_INC = 2.0 * PI * (t41.NCOFreq + sideToneShift) / t41.SampleRate;

  OSC_COS = cos(NCO_INC);
  OSC_SIN = sin(NCO_INC);

  for(i = 0; i < 2048; i++) {
    // generate local oscillator on-the-fly:  This takes a lot of processor time!
    Osc_Q = (Osc_Vect_Q * OSC_COS) - (Osc_Vect_I * OSC_SIN);  // Q channel of oscillator
    Osc_I = (Osc_Vect_I * OSC_COS) + (Osc_Vect_Q * OSC_SIN);  // I channel of oscillator
    Osc_Gain = 1.95 - ((Osc_Vect_Q * Osc_Vect_Q) + (Osc_Vect_I * Osc_Vect_I));  // Amplitude control of oscillator

    // rotate vectors while maintaining constant oscillator amplitude
    Osc_Vect_Q = Osc_Gain * Osc_Q;
    Osc_Vect_I = Osc_Gain * Osc_I;
    //
    // do actual frequency conversion
    float freqAdjFactor = 1.1;
    audioBufferL[i] = (audioBufferL_EX[i] * freqAdjFactor * Osc_Q) + (audioBufferR_EX[i] * freqAdjFactor * Osc_I); // multiply I/Q data by sine/cosine data to do translation
    audioBufferR[i] = (audioBufferR_EX[i] * freqAdjFactor * Osc_Q) - (audioBufferL_EX[i] * freqAdjFactor * Osc_I);
  }
}

/*****
  Calculate frequency spectrum

  *** 512 sample FFT assumed ***
  There isn't enough data available at high zoom levels and lower sample rates to perform the FFT.
  At those times, buffered data is used from previous calls to this routine for the FFT. When
  sufficient data is buffered the FFT is run and frequency spectrum is produced.

  *** the amount of data required by the frequency spectrum calc depends on the zoom factor ***
  calc passes needed to buffer a complete frequency spectrum at the current zoom factor
  and sample rate.  At 192kkHz sample rate, the zoom factor alone determines the passes
  required as the sample rate term below is 0.  At 44.1kHz sample rate, zoom is limited
  to 2x and 4x (22kHz/11kHz BW which is roughly equivalent to an 8x or 16x zoom).
  so the passes required based on zoom factor will always be 1 but the passes required
  based on sample rate are 4 or 8.

              <----------------- zoom factor -------------------------->   <----- sample rate ----->
  reqPasses = (t41.SpectrumZoom < 3 ? 1 : ((1 << t41.SpectrumZoom) / 4)) + 2048 / (blocks * 128) - 1;

  The IQ data gathered should consecutive to produce the highest resolution frequency spectrum

  Sample Rate: 192k
  Zoom    SpectrumZoom   BW   factor  dec 1   dec 2   passes  samples added per pass
   2x         1          96     2       2x      1x      1           512
   4x         2          48     2       2x      2x      1           512
   8x         3          24     4       4x      2x      2           256
  16x         4          12     8       8x      2x      4           128

  Sample Rate: 44.1k (FT8)
  Zoom    SpectrumZoom   BW   factor  dec 1   dec 2   passes  samples added per pass
   2x         1          22     2       2x      1x      1           512
   4x         2          11     2       2x      2x      1           512

*****/
bool CalcFreqSpecBuffered(uint32_t blockSize) {
  bool spectrumReady = false;
  float32_t multiplier;
  float32_t x_buffer[blockSize / 2];
  float32_t y_buffer[blockSize / 2];
  float32_t* ptrI = x_buffer;
  float32_t* ptrQ = y_buffer;
  int samples = blockSize / (1 << t41.SpectrumZoom);
  int factor = zoomDecFactors[t41.SpectrumZoom];

  static float32_t bufI[SPECTRUM_RES];
  static float32_t bufQ[SPECTRUM_RES];
  static int passes = 0;
  static int lastZoom = 1; // 2x - default zoom level

  // *** TODO: shouldn't be here for 1x zoom but sometimes are on vPS, likely
  //           due to bouncy switch matrix, investigate ***
  if(t41.SpectrumZoom == 0) return spectrumReady;

  SETPROFILEPIN(PROFILER_OTHER);

  // limit samples at lower resolutions
  if(samples > SPECTRUM_RES) samples = SPECTRUM_RES;

  // reset if zoom level has changed
  if(t41.SpectrumZoom != lastZoom) {
    lastZoom = t41.SpectrumZoom;
    passes = 0;
  }

  // set data multiplier according to zoom to maintain consistent spectrum level
  if(t41.SpectrumZoom > 3) { // SPECTRUM_ZOOM_8
    multiplier = (float32_t)(1 << t41.SpectrumZoom);
  } else {
    multiplier = (float32_t)t41.SpectrumZoom;
  }
  multiplier = 1.0f;

  // decimation 1
  arm_fir_decimate_f32(&Fir_Zoom_FFT_Decimate_I1, audioBufferL, x_buffer, blockSize);
  arm_fir_decimate_f32(&Fir_Zoom_FFT_Decimate_Q1, audioBufferR, y_buffer, blockSize);

  // decimation 2
  arm_fir_decimate_f32(&Fir_Zoom_FFT_Decimate_I2, x_buffer, x_buffer, blockSize / factor);
  arm_fir_decimate_f32(&Fir_Zoom_FFT_Decimate_Q2, y_buffer, y_buffer, blockSize / factor);

  // do we have enough data?
  // at high zoom, multiple, consecutive passes are required to get sufficient
  // data to produce a high resolution frequency spectrum
  int requiredPasses = t41.SpectrumZoom < 3 ? 1 : ((1 << t41.SpectrumZoom) / 4);
  if(passes < requiredPasses) {
    // multiple passes required, buffer IQ data
    arm_copy_f32(x_buffer, &bufI[passes * samples], samples);
    arm_copy_f32(y_buffer, &bufQ[passes * samples], samples);
    ptrI = bufI;
    ptrQ = bufQ;
  }
  if(++passes >= requiredPasses) {
    CalcFreqSpec(ptrI, ptrQ, multiplier);
    spectrumReady = true;
    passes = 0;
  }

  RESETPROFILEPIN(PROFILER_OTHER);

  return spectrumReady;
}

/*****
  Calcculate frequency spectrum

  *** FFT size fixed at 512 ***
*****/
FLASHMEM void CalcFreqSpec(float32_t* ptrI, float32_t* ptrQ, float multiplier /* = 1.0 */) {
  const arm_cfft_instance_f32* S = &arm_cfft_sR_f32_len512;
  static float32_t freqFFT[SPECTRUM_RES * 2]; // keep off stack
  static float32_t prevFreqSpecBuf[SPECTRUM_RES] = {0.0};
  static int lastZoom = 1; // 2x - default zoom level
  float32_t* dst = freqFFT;
  float32_t LPFcoeff, onem_LPFcoeff;

  // populate the FFT array
  for(int i = 0; i < SPECTRUM_RES; i++) {
    // interleave real and imaginary input values [real, imag, real, imag . . .]
    // apply data multiplier and Hann window
    float hann = multiplier * (0.5 - 0.5 * cos(6.28 * i / SPECTRUM_RES));
    *dst++ = ptrI[i] * hann;
    *dst++ = ptrQ[i] * hann;
  }

  // perform complex FFT
  arm_cfft_f32(S, freqFFT, 0, 1);

  // calculate frequency spectrum
  // calculate bin magnitude (I*I + Q*Q) and properly sequence + and - FFT bins
  // (swap positions of negative freq in second half of FFT with positive freq in first half of FFT)
  for(int i = 0; i < SPECTRUM_RES / 2; i++) {
    freqSpecBuf[i + SPECTRUM_RES / 2] = (freqFFT[i * 2] * freqFFT[i * 2] + freqFFT[i * 2 + 1] * freqFFT[i * 2 + 1]);
    freqSpecBuf[i] = (freqFFT[(i + SPECTRUM_RES / 2) * 2] * freqFFT[(i + SPECTRUM_RES / 2)  * 2] + freqFFT[(i + SPECTRUM_RES / 2)  * 2 + 1] * freqFFT[(i + SPECTRUM_RES / 2)  * 2 + 1]);
    // no swap version for testing
    //freqSpecBuf[i] = (freqFFT[i * 2] * freqFFT[i * 2] + freqFFT[i * 2 + 1] * freqFFT[i * 2 + 1]);
    //freqSpecBuf[i + SPECTRUM_RES/2]                  = (freqFFT[(i + SPECTRUM_RES/2) * 2] * freqFFT[(i + SPECTRUM_RES/2)  * 2] + freqFFT[(i + SPECTRUM_RES/2)  * 2 + 1] * freqFFT[(i + SPECTRUM_RES/2)  * 2 + 1]);
  }

  if(t41.SpectrumZoom != lastZoom) {
    lastZoom = t41.SpectrumZoom;
    CLEAR_VAR(prevFreqSpecBuf);
  } else if(!INJECT_RX_TEST_SIGNAL) {
    //if(t41.CalState == NOT_CAL_STATE)
    {
      // apply low pass filter to smooth spectrum (helps most w/ noise floor)
      LPFcoeff = 0.7;
      onem_LPFcoeff = 1.0 - LPFcoeff;

      for(int i = 0; i < SPECTRUM_RES; i++) {
        freqSpecBuf[i] = LPFcoeff * freqSpecBuf[i] + onem_LPFcoeff * prevFreqSpecBuf[i];
        prevFreqSpecBuf[i] = freqSpecBuf[i];
      }
    }
  }
}

/*****
  Process any updates to the following controls:
    Audio filter encoder
    Keyboard and mouse
    Course and Fine tune encoders
    Live menus
    Volume encoder

  Poll following T41 properties:

  *** called by YieldToProcess every ~10ms and less frequently by the main loop ***
*****/
// *** TODO: consider what controls are proper in various radio and display states and if to control them here ***
FASTRUN void ProcessControls() {
  bool updateDisplay = false;
  bool updateInfoBox = false;

  switch(t41.DisplayState) {
    case DISPLAY_T41:
    case DISPLAY_T41_FT8_DECODE:
      updateDisplay = true;
      updateInfoBox = true;
      break;

    case DISPLAY_FULL_MENU:
      updateInfoBox = true;
      break;

    case DISPLAY_BEACON_MONITOR:
    default:
    // no screen updates at all
      break;
  }

  // poll front pannel
  // *** inline function to poll front panel, empty function if not used ***
  PollFrontPanel();

  // handle USB Host
#if HOST_KEYBOARD_MOUSE_SUPPORT
  static unsigned long last_usb_read = 0;
  unsigned long now = millis();

  // poll USB Host at about every 8ms (125 Hz)
  if (now - last_usb_read > 8) {
    USBManager::getHost().Task();
    MouseLoop();
    last_usb_read = now;
  }
#endif

  // Handle tuning changes
  ProcessCenterTuneEncoder(READ_CENTERTUNE_ENCODER);

  // update filters if changed
  // *** TODO: examine if we can skip this comparison ***
  if(posFilterEncoder != lastFilterEncoder || filter_pos_BW != last_filter_pos_BW) {
    ProcessFilterEncoder();
  }

  if(resetTuningFlag) {
    // *** DrawBandwidthBar relies on resetTuningFlag being set prior to the ResetTuning call ***
    resetTuningFlag = false;
    ResetTuning();
  }

  // handle any live menu items
  if(getMenuValueActive) {
    if(getMenuSelected) {
      ptrMenuFollowup();

      // wrap up menu
      getMenuSelected = false;
      getMenuValueActive = false;
      ptrMenuLoop = NULL;
      ptrMenuFollowup = NULL;

      EraseMenus();
      menuStatus = NO_MENUS_ACTIVE;
    } else {
      GetMenuValueLoop();
    }
  }
  if(getMenuOptionActive) {
    if(getMenuSelected) {
      ptrMenuFollowup();

      // wrap up menu
      getMenuSelected = false;
      getMenuOptionActive = false;
      ptrMenuLoop = NULL;
      ptrMenuFollowup = NULL;

      EraseMenus();
      menuStatus = NO_MENUS_ACTIVE;
    } else {
      GetMenuOptionLoop();
    }
  }

  t41.Poll(updateDisplay);
  t41.PollInfoBox(updateInfoBox);

  UpdateClock();
  UpdateMemTempLoad();

  connectManager.update();
  catControl.update();
#if WSJT_USB_CAT_AUDIO
  wsjtControl.update();
#endif
}

#include <lwip/stats.h>

void EtherStats() {
  // Trigger a dump of all enabled statistics every 5 seconds
  static uint32_t lastPrint = 0;
  if (millis() - lastPrint > 5000) {
    lastPrint = millis();

    Serial.println("--- lwIP Statistics ---");
    stats_display();
    Serial.println("-----------------------");
  }
}

void YieldToProcess(bool updateSpectrum /* = false */) {
  static unsigned long lastControl = 0;
  unsigned long start = millis();
  unsigned long cal = millis();
  bool done = false;

  //EtherStats();

  // loop waiting on sufficient IQ data to process
  do {
    //TOGGLEPROFILEPIN(PROFILER_OTHER);

    // yield to ethernet traffic
    /*** Frequent calls here allow extra processing of Ethernet buffers.
         This creates some churn to the iqStream read/write methods but
         trying to regulate this call leads to unstable transfers.
         QNEthernet seems to handle this without problem, while calling
         at set intervals leads to data loss and/or freezes. ***/
    YieldToEthernet();

    if(updateSpectrum) {
      /* We go through here at the beginning of each loop to prepare
         spectrum data to be rendered that loop. We'll go through one
         extra time at 8x zoom and three extra times at 16x zoom to
         buffer sufficient data for the frequency spectrum
      */

      // break after frequency spectrum updated
      int result = CheckReceiverData(true);
      if(result == 2) {
        done = true;
      } else if(result == 3) {
        start = millis(); // restart no data timer
      }
      // break if no data in 10ms
      // this keeps the T41 from getting stuck here during testing
      // when updateSpectrum is true but there is no data to process
      if(millis() - start > 10) {
        done = true;
      }
    } else {
      // *** we go through here often ***

      // process IQ data while sufficient data exists
      // This allows the process to catch up after longer tasks
      // such as the waterfall update. Failing to do this can
      // result in poor audio.

      // break when insufficient IQ data is available
      if(CheckReceiverData() == 0) done = true;
    }

    // process controls every 10ms
    if(millis() - lastControl > 10) {
      ProcessControls();
      lastControl = millis();
    }
  } while(!done);

  // process controls every 10ms
  //if(millis() - lastControl > 10) {
  //  ProcessControls();
  //  lastControl = millis();
  //}
  if(t41.CalState == RXIQ_CAL_STATE) {
    if(millis() - cal > 8) {
      //PrepareExciterIQDataCal(1);
      cal = millis();
    }
  }
  //RESETPROFILEPIN(PROFILER_OTHER);
}

void YieldForProcess(int ms) {
  long unsigned entry = millis();

  // process controls and IQ data if 10ms has passed since last update
  while(millis() - entry < (long unsigned)ms) {
    YieldToProcess();
  }
}
