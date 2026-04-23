
#include "SDT.h"
#include "AudioConfig.h"
#include "Exciter.h"
//#include "EEPROM.h"
#include "Filter.h"
#include "FIR.h"
#include "keyer.h"
#include "Menu.h"
#include "Utility.h"

//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

#if T41_USB_AUDIO
extern AudioInputUSB usbIn;
#endif


//-------------------------------------------------------------------------------------------------------------
// Code
//-------------------------------------------------------------------------------------------------------------

/*****
  Purpose: Creates I and Q signals from single channel

    0.  Called with audioBufferL_EX filled with 256 bytes at 24kHz sample rate
    1.  Copy the L channel to the R channel
    2.  Process the R and L through Hilbert transformers - L 0deg phase shift and R 90 deg ph shift
          - This create the I (L) and Q(R) channels
    3.  Interpolate 8x (upsample and filter) the data stream to 192KHz sample rate
    4.  Output the data stream thruogh the DACs at 192KHz
*****/
void PlayExciterIQData() {
  int16_t *sp_L, *sp_R;
  int blocks = currentDemodMode == DEMOD_FT8 ? 2 : 16;

  // adjust IQ signal amplitude and phase
  // *** TODO: v66-9 has t41.CurrentBandA, why? ***
  if(currentDemodMode == DEMOD_LSB) {
    arm_scale_f32(audioBufferL_EX, IQXAmpCorrectionFactor[t41.CurrentBand], audioBufferL_EX, 256);
    IQPhaseCorrection(audioBufferL_EX, audioBufferR_EX, IQXPhaseCorrectionFactor[t41.CurrentBand], 256);
  } else if(currentDemodMode == DEMOD_USB || currentDemodMode == DEMOD_FT8_INTERNAL) {
    arm_scale_f32(audioBufferL_EX, -IQXAmpCorrectionFactor[t41.CurrentBand], audioBufferL_EX, 256);
    IQPhaseCorrection(audioBufferL_EX, audioBufferR_EX, IQXPhaseCorrectionFactor[t41.CurrentBand] * 2.0, 256);
  } else if(currentDemodMode == DEMOD_FT8) {
    arm_scale_f32(audioBufferL_EX, -IQXAmpCorrectionFactor[t41.CurrentBand], audioBufferL_EX, 256);
    IQPhaseCorrection(audioBufferL_EX, audioBufferR_EX, IQXPhaseCorrectionFactor[t41.CurrentBand] * 2.0, 256);
  }

  if(currentDemodMode != DEMOD_FT8) {
    // return to 192kHz, interpolate by a factor of 8, once again in two steps to preserve the spectrum order
    // 24kHz effective sample rate here
    arm_fir_interpolate_f32(&FIR_int1_EX_I, audioBufferL_EX, audioBufferTemp, 256);

    // interpolation-by-4,  48KHz effective sample rate here
    arm_fir_interpolate_f32(&FIR_int2_EX_I, audioBufferTemp, audioBufferL_EX, 512);

    // and again for R channel
    arm_fir_interpolate_f32(&FIR_int1_EX_Q, audioBufferR_EX, audioBufferTemp, 256);
    arm_fir_interpolate_f32(&FIR_int2_EX_Q, audioBufferTemp, audioBufferR_EX, 512);

    // 192kHz effective sample rate here

    // scale to compensate for losses during interpolation
    //arm_scale_f32(audioBufferL_EX, 8.0, audioBufferL_EX, blocks * 128);
    //arm_scale_f32(audioBufferR_EX, 8.0, audioBufferR_EX, blocks * 128);
    float pwr = pow(10, log10((float)transmitPowerLevel * 1000.0) / 2.0) / 31.62 * (4.0 * 1.0965);

    // *** currently pwr cal for FT8 internal ***
    arm_scale_f32(audioBufferL_EX, pwr / 32.168 / 2.0, audioBufferL_EX, blocks * 128);
    arm_scale_f32(audioBufferR_EX, pwr / 32.168 / 2.0, audioBufferR_EX, blocks * 128);
  } else {
    // measurements at dummy load tap which is -30dB with WSJT-X pwr level at -45dB
    // 2x scaler gives -6.6dbm
    // 4x scaler gives -0.8dbm
    // 12x scaler gives 8.1dbm
    // scale to 1W = 30dBm, scaller = 4.0 * 1.0965
    // scale to 5W = 36.99dBm, scaller = 4.0 * 2.4519
    float pwr = pow(10, log10((float)transmitPowerLevel * 1000.0) / 2.0) / 31.62 * (4.0 * 1.0965);
    arm_scale_f32(audioBufferL_EX, pwr, audioBufferL_EX, 256);
    arm_scale_f32(audioBufferR_EX, pwr, audioBufferR_EX, 256);
  }

  // convert to integer values and output
  for(int  i = 0; i < blocks; i++) {
    sp_L = Q_out_L_Ex.getBuffer();
    sp_R = Q_out_R_Ex.getBuffer();
    arm_float_to_q15(&audioBufferL_EX[128 * i], sp_L, 128);
    arm_float_to_q15(&audioBufferR_EX[128 * i], sp_R, 128);
    Q_out_L_Ex.playBuffer();
    Q_out_R_Ex.playBuffer();
  }

  // pause while this plays to prevent churn
  // *** TODO: find right pause interval ***
  //CWPause(10); // audio memory usage increases with this
  if(currentDemodMode != DEMOD_FT8) {
    CWPause(5); // audio memory usage doesn't increase with this
    //CWPause(10);
  }
}

void PrepareExciterIQData() {
  // *** we're at 24kHz sample rate here ***

  // copy left buffer to right channel
  arm_copy_f32(audioBufferL_EX, audioBufferR_EX, 256);

  if(currentDemodMode != DEMOD_FT8) {
    #if HILBERT_SIZE == 256 // 24kHz sample rate
    #ifdef USE_24K_SPS
      // create I and Q signals with Hilbert transform
      arm_fir_f32(&FIR_Hilbert_L, audioBufferL_EX, audioBufferL_EX, 256);
      arm_fir_f32(&FIR_Hilbert_R, audioBufferR_EX, audioBufferR_EX, 256);
    #else
      // use 12k sample rate with older Hilbert coefficients
      arm_fir_decimate_f32(&FIR_dec3_EX_I, audioBufferL_EX, audioBufferL_EX, 256);
      arm_fir_decimate_f32(&FIR_dec3_EX_Q, audioBufferR_EX, audioBufferR_EX, 256);

      arm_fir_f32(&FIR_Hilbert_L, audioBufferL_EX, audioBufferL_EX, 128);
      arm_fir_f32(&FIR_Hilbert_R, audioBufferR_EX, audioBufferR_EX, 128);

      // Interpolate back to 24kHz and scale to equalize levels
      // left channel first
      arm_fir_interpolate_f32(&FIR_int3_EX_I, audioBufferL_EX, audioBufferTemp, 128);
      arm_scale_f32(audioBufferTemp, 3.5, audioBufferL_EX, 256);

      // now right channel
      arm_fir_interpolate_f32(&FIR_int3_EX_Q, audioBufferR_EX, audioBufferTemp, 128);
      arm_scale_f32(audioBufferTemp, 3.5, audioBufferR_EX, 256);
    #endif
    #endif

    #if HILBERT_SIZE == 128 // 12kHz sample rate
      // v66-9 does the following:
      // Convert sample rate to 12kHz, apply Hilbert transforms and then interpolate back to 24kHz.
      // The objective is to extend the lower frequency range of the Hilbert transfrom by moving the
      // lowewr limit of the Hilbert usefullness down to below 200Hz.
      // Decimate by 2 to 12K SPS sample rate
      arm_fir_decimate_f32(&FIR_dec3_EX_I, audioBufferL_EX, audioBufferL_EX, 256);
      arm_fir_decimate_f32(&FIR_dec3_EX_Q, audioBufferR_EX, audioBufferR_EX, 256);

      // Hilbert transforms at 12kHz, with 5KHz bandwidth, buffer size 128
      arm_fir_f32(&FIR_Hilbert_L, audioBufferL_EX, audioBufferL_EX, 128);
      arm_fir_f32(&FIR_Hilbert_R, audioBufferR_EX, audioBufferR_EX, 128);

      // Interpolate back to 24kHz and scale to equalize levels
      // left channel first
      arm_fir_interpolate_f32(&FIR_int3_EX_I, audioBufferL_EX, audioBufferTemp, 128);
      arm_scale_f32(audioBufferTemp, 3.5, audioBufferL_EX, 256);

      // now right channel
      arm_fir_interpolate_f32(&FIR_int3_EX_Q, audioBufferR_EX, audioBufferTemp, 128);
      arm_scale_f32(audioBufferTemp, 3.5, audioBufferR_EX, 256);
    #endif
  } else {
    // create I and Q signals with Hilbert transform
    arm_fir_f32(&FIR_Hilbert_L, audioBufferL_EX, audioBufferL_EX, 256);
    arm_fir_f32(&FIR_Hilbert_R, audioBufferR_EX, audioBufferR_EX, 256);
  }

  PlayExciterIQData();
}

/*****
  Purpose: Gets data from Mic input

    Notes:
    There are several actions in this function
    1.  Read in the data from the ADC into the Left Channel at 192KHz
    2.  Format the L data and decimate (downsample and filter) the sampled data by x8
          - the new effective sampling rate is now 24kHz (or 44.1kHz for FT8)
    3.  Process the L data through the 7 EQ filters and combine to a single data stream
    4.  Create and play IQ signals with PrepareExciterIQData
*****/
void PrepareMicExciterData() {
  int16_t *sp_L;
  int blocks = currentDemodMode == DEMOD_FT8 ? 2 : 16;

  // process samples from queue buffer if there are at least 16 buffers available
  if(Q_in_L_Ex.available() > blocks) {
    // get audio samples from the audio  buffers and convert them to float
    for(int i = 0; i < blocks; i++) {
      // read in 16 blocks á 128 samples into the left channel, we'll duplicate this later
      sp_L = Q_in_L_Ex.readBuffer();

      // convert to float one buffer_size, samples are now standardized from > -1.0 to < 1.0
      arm_q15_to_float(sp_L, &audioBufferL_EX[128 * i], 128);

      Q_in_L_Ex.freeBuffer();
    }

    /**********************************************************************************
              Decimation is the process of downsampling the data stream and LP filtering
              Decimation is done in two stages to prevent reversal of the spectrum, which occure with each even
              Decimation.  First select every 4th asmple and then every 2nd sample, yielding 8x downsampling
              192KHz/8 = 24KHz, with 8xsmaller sample sizes
     **********************************************************************************/

    if(currentDemodMode != DEMOD_FT8) {
      // reduce sample rate and size by decimation by 8
      // decimate in two stages to maintain spectrum order
      // 192kHz effective sample rate here
      // decimation-by-4 in-place
      arm_fir_decimate_f32(&FIR_dec1_EX_I, audioBufferL_EX, audioBufferL_EX, 2048);

      // 48KHz effective sample rate here
      // decimation-by-2 in-place
      arm_fir_decimate_f32(&FIR_dec2_EX_I, audioBufferL_EX, audioBufferL_EX, 512);
    } else {
      float tmp[256];

      // let's listen to it
      // *** this gives reasonable volume at wsjt-x -45dB transmit level and 1 Windows mixer volume setting ***
      // *** higher wsjt-x transmit levels (~-20dB) is too loud here ***
      // *** TODO: higher wsjt-x transmit levels  do not increase power out ***
      // *** TODO: add volume adjustment here ***
      #if T41_USB_AUDIO
        arm_scale_f32(audioBufferL_EX, usbIn.volume() / 0.01, tmp, blocks * 128);
      #else
        arm_scale_f32(audioBufferL_EX, 1.0, tmp, blocks * 128);
      #endif
      sp_L = Q_out_L.getBuffer();
      arm_float_to_q15(tmp, sp_L, blocks * 128);
      Q_out_L.play(sp_L, blocks * 128);
    }

    // leaving us at 24kHz sample rate here

    // perform transmit EQ if activated
    if(xmitEQFlag == ON) {
      DoExciterEQ();
    }

    PrepareExciterIQData();
  }
}

/*****
  Purpose: Set the current band relay ON or OFF

  Parameter list:
    int state             OFF = 0, ON = 1
*****/
void SetBandRelay(int state) {
  // There are 4 physical relays.  Turn all of them off.
  for(int i = 0; i < 4; i = i + 1) {
    digitalWrite(bandswitchPins[i], LOW); // set ALL band relays low
  }

  // Set current band relay "on".  Ignore 12M and 10M.  15M and 17M use the same relay.
  if(t41.CurrentBand < BAND_12M) digitalWrite(bandswitchPins[t41.CurrentBand], state);
}

// *** TODO: see if this can be refactored from above ***
void PrepareFT8ExciterIQData(float *sig) {
  // *** we're at 12kHz sample rate here ***

  // copy signal to left and right channels
  arm_copy_f32(sig, audioBufferL_EX, 128);
  arm_copy_f32(sig, audioBufferR_EX, 128);

  // create I and Q signals with Hilbert transform
  arm_fir_f32(&FIR_Hilbert_L, audioBufferL_EX, audioBufferL_EX, 128);
  arm_fir_f32(&FIR_Hilbert_R, audioBufferR_EX, audioBufferR_EX, 128);

  // Interpolate  to 24kHz and scale to equalize levels
  // left channel first
  arm_fir_interpolate_f32(&FIR_int3_EX_I, audioBufferL_EX, audioBufferTemp, 128);
  //arm_scale_f32(audioBufferTemp, 3.5, audioBufferL_EX, 256);
  arm_scale_f32(audioBufferTemp, 2.0, audioBufferL_EX, 256);

  // now right channel
  arm_fir_interpolate_f32(&FIR_int3_EX_Q, audioBufferR_EX, audioBufferTemp, 128);
  //arm_scale_f32(audioBufferTemp, 3.5, audioBufferR_EX, 256);
  arm_scale_f32(audioBufferTemp, 2.0, audioBufferR_EX, 256);

  PlayExciterIQData();
}
