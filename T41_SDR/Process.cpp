
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
#include "Noise.h"
#include "Process.h"
#include "psk31.h"
#include "Tune.h"
//#include "t41Control.h"
#include "t41USBHost.h"
#include "Utility.h"

#include "debug.h"

#include "radio.h"
#include "connectManager.h"

//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

#if DEVICE_REMOTE_OPS_MODE > 0
extern ConnectManager transport;
#endif
extern RemoteRadio radio;

// *** TODO: this is display dependent, but also fundamental to much of how the DSP process works ***
#define SPECTRUM_RES          512

bool FFTupdated;

arm_biquad_casd_df1_inst_f32 biquad_lowpass1;
float32_t biquad_lowpass1_state[4];
float32_t biquad_lowpass1_coeffs[5] = { 0, 0, 0, 0, 0 };

float32_t audioMaxSquaredAve = 0.01; // this will blow up dBm if 0

float32_t audioSpectBuffer[1024]; // This can't be DMAMEM.  It will break the S-Meter. *** TODO: probably because it needs to be aligned ***

uint8_t NB_on = 0; // noise blanker: 0 - off, 1 - on

char atom, currentAtom;
uint8_t ANR_notch = 0;
uint8_t ANR_notchOn = 0;

float32_t DMAMEM audioFFT[1024] __attribute__((aligned(4)));
float32_t DMAMEM audioIFFT[1024 + 1];
float32_t DMAMEM prevAudioBuffer_L[256];
float32_t DMAMEM prevAudioBuffer_R[256];

#define USE_LOG10FAST

int zoom_sample_ptr = 0;

float32_t DMAMEM freqFFT[1024] __attribute__((aligned(4)));
float32_t DMAMEM freqSpecBuf[1024];
float32_t DMAMEM prevFreqSpecBuf[1024];

extern arm_fir_decimate_instance_f32 Fir_Zoom_FFT_Decimate_I1, Fir_Zoom_FFT_Decimate_Q1, Fir_Zoom_FFT_Decimate_I2, Fir_Zoom_FFT_Decimate_Q2;

//-------------------------------------------------------------------------------------------------------------
// Forwards
//-------------------------------------------------------------------------------------------------------------

float VolumeToAmplification(int volume);
void FreqShift1(int blockSize);
void FreqShift2();
void CalcZoomFreqSpec(uint32_t blockSize, bool updateSpectrumData);
void Calc1xFreqSpec();

//-------------------------------------------------------------------------------------------------------------
// Code
//-------------------------------------------------------------------------------------------------------------

void InitFFTArrays() {
  CLEAR_VAR(audioFFT);
  CLEAR_VAR(prevAudioBuffer_L);
  CLEAR_VAR(prevAudioBuffer_R);

  CLEAR_VAR(freqFFT);
  CLEAR_VAR(freqSpecBuf);
  CLEAR_VAR(prevFreqSpecBuf);
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
  q15_t q15_buffer_LTemp[2048];
  float rfGainValue, intScaler;
  // audio spectrum calc works with 256 samples which is 2 blocks at 44.1kHz or 16 blocks at 192kHz decimated by 8 or 24Hz
  int blocks = t41.DemodMode == DEMOD_FT8 ? 2 : 16;
  // *** the amount of data required by the frequency spectrum calc depends on the zoom factor ***
  static int reqPasses = 20;
  static int passes = 20;
  bool updateFreqSpec = false; // true: spectrums updated, otherwise false

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

  // get audio samples from the audio buffers and convert them to float
  // read I and Q blocks into buffers (128 samples each)
  for(int i = 0; i < blocks; i++) {
    /**********************************************************************************
        Using arm_Math library, convert to float one buffer_size.
        Float_buffer samples are now standardized from > -1.0 to < 1.0
    **********************************************************************************/
    arm_q15_to_float(Q_in_R.readBuffer(), &audioBufferL[128 * i], 128);
    arm_q15_to_float(Q_in_L.readBuffer(), &audioBufferR[128 * i], 128);

    Q_in_L.freeBuffer();
    Q_in_R.freeBuffer();
  }

  // *** TODO: consider if this is needed for FT8 ***
  // set RF gain for all bands
  rfGainValue = pow(10, (float)t41.RFGain / 20);
  arm_scale_f32(audioBufferL, rfGainValue, audioBufferL, blocks * 128);
  arm_scale_f32(audioBufferR, rfGainValue, audioBufferR, blocks * 128);

  /**********************************************************************************
      Remove DC offset to reduce centeral spike.  First read the Mean value of
      left and right channels.  Then fill L and R correction arrays with those Means
      and subtract the Means from the float L and R buffer data arrays.  Again use Arm_Math functions
      to manipulate the arrays.  Arrays are all 2048 long
  **********************************************************************************/
  switch(t41.DemodMode) {
    //case DEMOD_FT8:
    //  break;

    default:
      RemoveDCBias();
      break;
  }

  /**********************************************************************************
      Scale the data buffers by the rfGain value defined in bands[t41.ActiveBand] structure
  **********************************************************************************/
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
    //  Serial.println("clearing @ ProcessReceiverData ...");
    //}
    Q_in_L.clear();
    Q_in_R.clear();
    t41.DroppedBlock = 1;
  }

  /**********************************************************************************
    IQ amplitude and phase correction.  For this scaled down version the I an Q channels are
    equalized and phase corrected manually. This is done by applying a correction, which is the difference, to
    the L channel only.  The phase is corrected in the IQPhaseCorrection() function.

    IQ amplitude and phase correction
  ***********************************************************************************************/

  // Manual IQ amplitude correction
  if(t41.DemodMode == DEMOD_LSB || t41.DemodMode == DEMOD_AM || t41.DemodMode == DEMOD_SAM || t41.DemodMode == DEMOD_NFM) {
    arm_scale_f32(audioBufferL, -IQAmpCorrectionFactor[t41.ActiveBand], audioBufferL, blocks * 128);
    IQPhaseCorrection(audioBufferL, audioBufferR, -IQPhaseCorrectionFactor[t41.ActiveBand], blocks * 128);
  //} else if(t41.DemodMode == DEMOD_USB || t41.DemodMode == DEMOD_AM || t41.DemodMode == DEMOD_SAM || t41.DemodMode == DEMOD_FT8) {
  } else {
    arm_scale_f32(audioBufferL, -IQAmpCorrectionFactor[t41.ActiveBand], audioBufferL, blocks * 128);
    IQPhaseCorrection(audioBufferL, audioBufferR, IQPhaseCorrectionFactor[t41.ActiveBand], blocks * 128);
  }

  /**********************************************************************************
      Perform a 256 point FFT for the spectrum display on the basis of the first 256 complex values
      of the raw IQ input data this saves about 3% of processor power compared to calculating
      the magnitudes and means of the 4096 point FFT for the display

      Only go there from here, if magnification == 1
  ***********************************************************************************************/

  if((t41.SpectrumZoom == 0) && updateSpectrumData) {
    Calc1xFreqSpec();
    updateFreqSpec = true;

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
      shifted by Fs/4.  Buffering and processing is done in the CalcZoomFreqSpec function.
  **********************************************************************************/
  // Kick off frequency spectrum FFT routine only once for each audio process loop
  if(t41.SpectrumZoom != 0) {
    if(updateSpectrumData && (reqPasses == 20)) {
      passes = 0;

      // calc passes needed to buffer a complete frequency spectrum at the current zoom factor
      // and sample rate.  At 192kkHz sample rate, the zoom factor alone determines the passes
      // required as the sample rate term below is 0.  At 44.1kHz sample rate, zoom is limited
      // to 2x and 4x (22kHz/11kHz BW which is roughly equivalent to an 8x or 16x zoom).
      // so the passes required based on zoom factor will always be 1 but the passes required
      // based on sample rate are 4 or 8.
      //          <----------------- zoom factor ------------------>   <----- sample rate ----->
      reqPasses = (t41.SpectrumZoom < 3 ? 1 : ((1 << t41.SpectrumZoom) / 4)) + 2048 / (blocks * 128) - 1;
    }
    if(passes < reqPasses) {
      passes++;
      if(passes == reqPasses) {
        // flag that we're ready to update frequency spectrum
        // no need to reset passes, we won't pass through this
        // block again until the next time updateSpectrumData is set
        updateFreqSpec = true;
        reqPasses = 20;
        passes = 20;
      }
      CalcZoomFreqSpec(blocks * 128, updateFreqSpec);
    }
  }

  // *** TODO: this is from v11 - reconcile calibration calls within Process.cpp ***
  //if(calibrateItem >= 0) {
  //  //CalibrateOptions();
  //}

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
      SETPROFILEPIN(PROFILER_FT8_REMOTE_RX);
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
      RESETPROFILEPIN(PROFILER_FT8_REMOTE_RX);
      break;
    #endif

    case DEMOD_FT8_WAV:
      // get samples from wav file (assumed open)
      // pull data at the same rate as the T41
      // audio is 256 bytes, 24 kHz at this point
      // wav file sample rate is 12 kHz
      // get a half sized sample and interpolate to the proper size/rate
      // wav FT8 signal data to audioBufferR, audio to audioBufferL
      SETPROFILEPIN(PROFILER_FT8_REMOTE_RX);
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
      RESETPROFILEPIN(PROFILER_FT8_REMOTE_RX);
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
    //  AudioDSP(updateFreqSpec);
    //  break;

    #ifdef USE_BUFFERED_FT8_WAV
    case DEMOD_FT8_INTERNAL: // *** TODO: this is USE_BUFFERED_FT8_WAV only ***
    #endif
    case DEMOD_FT8_WAV:
      //AudioDSP(updateFreqSpec, 20, false); // no imaginary component for these
      AudioDSP(updateFreqSpec, false); // no imaginary component for these
      break;

    default:
      // prepare audio signals for all other modes
      AudioDSP(updateFreqSpec);

      // apply automatic gain control
      // AGC acts upon on the audio data in audioIFFT
      // *** TODO: evaluate effectiveness and proper placement of this AGC function ***
      AGC();
      break;
  }

  YieldToEthernet();

  /**********************************************************************************
    Demodulation
      our time domain output is a combination of the real part (left channel) AND the imaginary part (right channel) of the second half of the audioFFT
      The demod mode is accomplished by selecting/combining the real and imaginary parts of the output of the IFFT process.
  **********************************************************************************/
  switch(t41.DemodMode) {
    case DEMOD_AM:
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
      AudioDSP(updateFreqSpec, false); // no imaginary component for these

      // apply automatic gain control
      // AGC acts upon on the audio data in audioIFFT
      // *** TODO: evaluate effectiveness and proper placement of this AGC function ***
      AGC();
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
  //if(updateFreqSpec && controlDataFlag) {
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
      ANR_notch = 0;
      Xanr();
      arm_scale_f32(audioBufferL, 1.5, audioBufferL, 256);
      //arm_scale_f32(audioBufferR, 2, audioBufferR, 256);
      break;
  }

  // apply automatic notch if set
  if(ANR_notchOn == 1) {
    ANR_notch = 1;
    Xanr();
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
  arm_float_to_q15(audioBufferL, q15_buffer_LTemp, blocks * 128);
  Q_out_L.play(q15_buffer_LTemp, blocks * 128);

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

  return updateFreqSpec ? 2 : 1;
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

  switch(displayState) {
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
#ifdef USB_HOST_SUPPORT
  static unsigned long last_usb_read = 0;
  unsigned long now = millis();

  // poll USB Host at about every 8ms (125 Hz)
  if (now - last_usb_read > 8) {
    UsbHostLoop();
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

  transport.update();
  radio.update();
}

/*****
  Purpose:
        Frequency translation by Fs/4 without multiplication from Lyons (2011): chapter 13.1.2 page 646
        together with the savings of not having to shift/rotate the audioFFT, this saves
        about 1% of processor use

        This is for +Fs/4 [moves receive frequency to the left in the spectrum display]
           audioBufferL contains I = real values
           audioBufferR contains Q = imaginary values
           xnew(0) =  xreal(0) + jximag(0)
               leave first value (DC component) as it is!
           xnew(1) =  - ximag(1) + jxreal(1)
           ...

      Backward shift:
      This is for -Fs/4 [moves receive frequency to the right in the spectrumdisplay]
      // shift backward Fs/4
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
    // shift forward Fs/4
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
  Purpose: Shift Receive frequency by an arbitray amount
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

  if(t41.RadioMode == SSB_MODE || t41.RadioMode == DATA_MODE) {
    sideToneShift = 0;
  } else {
    if(t41.RadioMode == CW_MODE ) {
      if(t41.DemodMode == 1) {
        sideToneShift = CWFreqShift;
      } else {
        if(t41.DemodMode == 0) {
          sideToneShift = -CWFreqShift;
        }
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
  Purpose: Calculate frequency spectrum
           Intended for t41.SpectrumZoom > 1

           *** TODO: process here is poorly explained, fix ***
*****/
void CalcZoomFreqSpec(uint32_t blockSize, bool updateSpectrumData) {
  float32_t LPFcoeff;
  float32_t onem_LPFcoeff;
  float32_t x_buffer[blockSize];
  float32_t y_buffer[blockSize];
  static float32_t FFT_ring_buffer_x[SPECTRUM_RES * 2];
  static float32_t FFT_ring_buffer_y[SPECTRUM_RES * 2];
  int sample_no = blockSize / (1 << t41.SpectrumZoom);
  float32_t multiplier = (float32_t)t41.SpectrumZoom;;
  const arm_cfft_instance_f32* S = &arm_cfft_sR_f32_len512;
  int factor = t41.SpectrumZoom < 3 ? 2 : (1 << t41.SpectrumZoom) / 2;

  if (sample_no > SPECTRUM_RES) {
    sample_no = SPECTRUM_RES;
  }

  // decimation 1
  arm_fir_decimate_f32(&Fir_Zoom_FFT_Decimate_I1, audioBufferL, x_buffer, blockSize);
  arm_fir_decimate_f32(&Fir_Zoom_FFT_Decimate_Q1, audioBufferR, y_buffer, blockSize);

  // decimation 2
  arm_fir_decimate_f32(&Fir_Zoom_FFT_Decimate_I2, x_buffer, x_buffer, blockSize / factor);
  arm_fir_decimate_f32(&Fir_Zoom_FFT_Decimate_Q2, y_buffer, y_buffer, blockSize / factor);


  if(zoom_sample_ptr >= SPECTRUM_RES) zoom_sample_ptr = 0;

  // put sample_no samples into the circular buffer
  for(int i = 0; i < sample_no; i++) {
    FFT_ring_buffer_x[zoom_sample_ptr] = x_buffer[i];
    FFT_ring_buffer_y[zoom_sample_ptr] = y_buffer[i];
    zoom_sample_ptr++;
    if(zoom_sample_ptr >= SPECTRUM_RES) zoom_sample_ptr = 0;
  }

  // populate the FFT array and apply a Hanning window
  // the right order has to be thought about!
  // we take all the samples from zoom_sample_ptr to 256 and
  // then all samples from 0 to zoom_sampl_ptr - 1
  if(t41.SpectrumZoom > 3) { // SPECTRUM_ZOOM_8
    multiplier = (float32_t)(1 << t41.SpectrumZoom);
  }
  for(int idx = 0; idx < SPECTRUM_RES; idx++) {
    // interleave real and imaginary input values [real, imag, real, imag . . .]
    // apply Hanning window
    freqFFT[idx * 2 + 0] =  multiplier * FFT_ring_buffer_x[zoom_sample_ptr] * (0.5 - 0.5 * cos(6.28 * idx / SPECTRUM_RES));
    freqFFT[idx * 2 + 1] =  multiplier * FFT_ring_buffer_y[zoom_sample_ptr] * (0.5 - 0.5 * cos(6.28 * idx / SPECTRUM_RES));
    zoom_sample_ptr++;
    if(zoom_sample_ptr >= SPECTRUM_RES) {
      zoom_sample_ptr = 0;
    }
  }
  //***************
  // adjust lowpass filter coefficient, so that
  // "spectrum display smoothness" is the same across the different sample rates
  // and the same across different magnify modes . . .
  LPFcoeff = 0.7;

  //if(LPFcoeff > 1.0) {
  //  LPFcoeff = 1.0;
  //}
  //if(LPFcoeff < 0.001) {
  //  LPFcoeff = 0.001;
  //}

  onem_LPFcoeff = 1.0 - LPFcoeff;

  // perform complex FFT
  // calculation is performed in-place the audioFFT [re, im, re, im, re, im . . .]
  arm_cfft_f32(S, freqFFT, 0, 1);

  for(int i = 0; i < SPECTRUM_RES / 2; i++) {
    freqSpecBuf[i + SPECTRUM_RES / 2] = (freqFFT[i * 2] * freqFFT[i * 2] + freqFFT[i * 2 + 1] * freqFFT[i * 2 + 1]); // Last half of spectrum
    freqSpecBuf[i] = (freqFFT[(i + SPECTRUM_RES / 2) * 2] * freqFFT[(i + SPECTRUM_RES / 2)  * 2] + freqFFT[(i + SPECTRUM_RES / 2)  * 2 + 1] * freqFFT[(i + SPECTRUM_RES / 2)  * 2 + 1]);
  }

  if(updateSpectrumData) {
    // apply low pass filter and scale the magnitude values and convert to int for spectrum display
    // apply spectrum AGC
    //int16_t min = 0;
    //int16_t max = 0;
    //int16_t data[SPECTRUM_RES];

    for(int i = 0; i < SPECTRUM_RES; i++) {
      freqSpecBuf[i] = LPFcoeff * freqSpecBuf[i] + onem_LPFcoeff * prevFreqSpecBuf[i];
      prevFreqSpecBuf[i] = freqSpecBuf[i];

      //if(controlDataFlag) {
      //  // *** TODO: reconsider transfers to PC control app ***
      //  // hardwire for 10dB scale, 20 pixel offset, 20 dBScale
      //  int16_t pixelnew = FREQSPEC_OFFSET_10DB + 20 + (20 * log10f_fast(freqSpecBuf[i]));
//
      //  // *** control app data no longer has current noise floor as that is display dependent ***
      //  data[i] = pixelnew;
      //  if(data[i] < min) {
      //    min = data[i];
      //  }
      //  if(data[i] > max) {
      //    max = data[i];
      //  }
      //}
    }

    // set up specData for frequency spectrum command if applicable
    //if(controlDataFlag) {
    //  T41PrepareSpectrumData(data, max);
    //}
    //if(connected) {
    //  int tmp = 0;
    //  for(int i = 0; i < SPECTRUM_RES; i++) {
    //    // shift data so max = 255
    //    // *** TODO: consider scaling here fits data into a 0-255 range ***
    //    tmp = SPECTRUM_NOISE_FLOOR - pixelnew[i] - currentNF;
    //    // though unlikely, data can still be negative, limit it
    //    if(tmp < 0) {
    //      tmp = SPECTRUM_BOTTOM;
    //    }
    //    if(tmp > 255) {
    //      tmp = SPECTRUM_BOTTOM;
    //    }
    //    freqData[i] = tmp;
    //  }
    //}
  }
}

/*****
  Purpose: Calcculate zoom magnification when Spectrum Zoom = 1
*****/
// *** updateSpectrumData is assumed true ***
void Calc1xFreqSpec() {
  const arm_cfft_instance_f32* S = &arm_cfft_sR_f32_len512;
  float32_t spec_help = 0.0;
  float32_t LPFcoeff = 0.7;

  if(LPFcoeff > 1.0) {
    LPFcoeff = 1.0;
  }

  for(int i = 0; i < SPECTRUM_RES; i++) { // interleave real and imaginary input values [real, imag, real, imag . . .]
    freqFFT[i * 2] =      audioBufferL[i] * (0.5 - 0.5 * cos(6.28 * i / SPECTRUM_RES)); //Hanning
    freqFFT[i * 2 + 1] =  audioBufferR[i] * (0.5 - 0.5 * cos(6.28 * i / SPECTRUM_RES));
  }
  // perform complex FFT
  // calculation is performed in-place the audioFFT [re, im, re, im, re, im . . .]
  arm_cfft_f32(S, freqFFT, 0, 1);

  // calculate magnitudes and put into freqSpecBuf
  // we do not need to calculate magnitudes with square roots, it would seem to be sufficient to
  // calculate mag = I*I + Q*Q, because we are doing a log10-transformation later anyway
  // and simultaneously put them into the right order
  // 38.50%, saves 0.05% of processor power and 1kbyte RAM ;-)

  for(int i = 0; i < SPECTRUM_RES/2; i++) {
      freqSpecBuf[i + SPECTRUM_RES/2] = (freqFFT[i * 2] * freqFFT[i * 2] + freqFFT[i * 2 + 1] * freqFFT[i * 2 + 1]);
      freqSpecBuf[i]                  = (freqFFT[(i + SPECTRUM_RES/2) * 2] * freqFFT[(i + SPECTRUM_RES/2)  * 2] + freqFFT[(i + SPECTRUM_RES/2)  * 2 + 1] * freqFFT[(i + SPECTRUM_RES/2)  * 2 + 1]);
    }

  // apply low pass filter and scale the magnitude values and convert to int for spectrum display
  for(int x = 0; x < SPECTRUM_RES; x++) {
    spec_help = LPFcoeff * freqSpecBuf[x] + (1.0 - LPFcoeff) * prevFreqSpecBuf[x];
    prevFreqSpecBuf[x] = spec_help;
  }
}

void YieldToProcess(bool updateSpectrum /* = false */) {
  static long prevUpdate = 0;

  while(true) {
    YieldToEthernet();
    if(updateSpectrum) {
      // wait for spectrum data update
      if(CheckReceiverData(true) == 2) break;
    } else {
      // process IQ data while sufficient data exists
      // This allows the process to catch up after longer tasks
      // such as the waterfall update. Failing to do this can
      // result in poor audio.
      if(CheckReceiverData() != 1) break;
    }
    // process controls if 10ms has passed since last update
    if(millis() - prevUpdate > 10) {
      ProcessControls();
      prevUpdate = millis();
      //if(++count > 10) {
      //  // prevent freeze when no input is present
      //  // *** TODO: revisit this ***
      //  count = 0;
      //  break;
      //}
    }
  }
}

void YieldForProcess(int ms) {
  long unsigned entry = millis();

  // process controls and IQ data if 10ms has passed since last update
  while(millis() - entry < (long unsigned)ms) {
    YieldToProcess();
  }
}
