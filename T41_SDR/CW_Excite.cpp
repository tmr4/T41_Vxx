
#include "SDT.h"
#include "AudioConfig.h"
#include "Exciter.h"
#include "FIR.h"
#include "keyer.h"
#include "Utility.h"

//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

// keyPressedOn set to 1 in ISR indicating a dit or dah key press in CW mode.
// This should be reset to 0 after processing the key click.
// I've removed check for this being set in ShowSpectrum and ProcessReceiverData.
// This means there will be an increased lag from when the CW key is pressed
// until it is recognized in the next processing loop after the current loop completes.
// I haven't noticed this, perhaps because a more efficient processing loop.
// *** TODO: consider reintroducing an early return from longer running process loop
//     code if lag becomes noticable. ***
uint8_t keyPressedOn = 0;

// pwrScale scales CW signal for: true=pwr out eqn, false=cal factor only
bool pwrScale = true;

//-------------------------------------------------------------------------------------------------------------
// Code
//-------------------------------------------------------------------------------------------------------------

/*****
  Purpose: CW key interrupt
*****/
void KeyTipOn() {
  if(digitalRead(KEYER_DIT_INPUT_TIP) == LOW && radioMode == CW_MODE ) {
    keyPressedOn = 1;
  }
}

/*****
  Purpose: CW key interrupt
*****/
void KeyRingOn() {
  if(keyType == 1) {
    if(digitalRead(KEYER_DAH_INPUT_RING) == LOW && radioMode == CW_MODE ) {
      keyPressedOn = 1;
    }
  }
}

/*****
  Purpose: Create and play I and Q sample for CW signal
           This creates a 10ms, 750 Hz sample at 192 kHz sample rate to the Teensy Audio Adapter line-out to
           the exciter board.  Function must be called again within that time for a continuous signal.  Prior to call
           Q_out_L_Ex, Q_out_R_Ex and Q_out_L (for sidetone) must be properly routed.  Sidetone signal adjusted for a gain of 1
           at a volume of 30.  Signal level is controlled by powerOutCW[currentBand] and volumeLog[sidetoneVolume] / 0.000100.
           This gives a reasonable volume with power level of 1-20 W.  This should be done prior to calling this function or CreateCWSignal.

  Parameter list:
    int state       turn signal ON or OFF
    bool ramp       add a ramp (upwards for on, downwards for off)
    bool pause      pause for 10ms while signal plays
    int timeAdjust  shorten the ramp block by timeAdjust ms
*****/
void CW_ExciterIQData(int state = ON, bool ramp = false, bool pause = true, float timeAdjust = 0.0) {
  double tp = transmitPowerLevel;
  double cwPwr;
  float fac;
  //float cwPwr = (pwrScale ? (-.0133 * transmitPowerLevel * transmitPowerLevel + .7884 * transmitPowerLevel + 4.5146) * CWPowerCalibrationFactor[currentBand] / CWPowerCalibrationFactor[1] : 8.0);
  //float cwPwr = (pwrScale ? (-.0133 * transmitPowerLevel * transmitPowerLevel + .7884 * transmitPowerLevel + 4.5146) : 8.0);
  // using Pt=1W measurement
  //float cwPwr = (pwrScale ? ( pow(10.0, 0.5 * log10((float)transmitPowerLevel)) * 0.78938 * CWPowerCalibrationFactor[currentBand]) : 8.0);
  // using Pt=10W measurement
  //float cwPwr = (pwrScale ? ( pow(10.0, 0.5 * log10((float)transmitPowerLevel / 10.0)) * 3.5459 * CWPowerCalibrationFactor[currentBand]) : 8.0);
  //float cwPwr = (pwrScale ? ( pow(10.0, 0.5 * log10((float)transmitPowerLevel / 10.0)) * 3.5459) : 8.0);
  // using theoretical pwr to voltage formula
  //float cwPwr = (pwrScale ? (.70711 * pow(transmitPowerLevel, 0.5)) : 8.0);
  // using empirical pwr to voltage formula
  //float cwPwr = (pwrScale ? (.675 * pow(transmitPowerLevel, 0.5552) / 5.25 * CWPowerCalibrationFactor[currentBand]) : 8.0);
  //float cwPwr = (pwrScale ? (7.0711 * pow(transmitPowerLevel, 0.5) * CWPowerCalibrationFactor[currentBand]) : 8.0);
  // empirical formula y = -0.0002x4 + 0.0062x3 - 0.0653x2 + 0.4673x + 0.2741
  // works well with dummy load at 7MHz considering the tap atten of -20dB
  //float cwPwr = (pwrScale ? (-.0002 * pow(tp, 4.0) + 0.0062 * pow(tp, 3.0) - 0.0653 * pow(tp, 2.0) + 0.4673 * tp + 0.2741) : 8.0);
  // however that gives a 41.3dBm at Pt=1
  //float cwPwr = (pwrScale ? ((-.0002 * pow(tp, 4.0) + 0.0062 * pow(tp, 3.0) - 0.0653 * pow(tp, 2.0) + 0.4673 * tp + 0.2741) / 5.58) : 8.0);
  //float cwPwr = CWPowerCalibrationFactor[currentBand];
  // y = 6.3749x^5 - 154.46x^4 + 1437.3x^3 - 6384.5x^2 + 17189x + 962.75
  //float cwPwr = (6.3749 * pow(tp, 5.0) - 154.46 * pow(tp, 4.0) + 1437.3 * pow(tp, 3.0) - 6384.5 * pow(tp, 2.0) + 17189.0 * tp + 962.75) / 100000.0;

  //Serial.println(cwPwr);

  // create I/Q from precalculated buffers (750 Hz signal at a 24kHz sample rate)
  arm_scale_f32(sinBuffer2, 1.0, audioBufferL_EX, 256);
  arm_scale_f32(cosBuffer2, 1.0, audioBufferR_EX, 256);
  //arm_scale_f32(sinBuffer2, 0.5, audioBufferL_EX, 256);
  //arm_scale_f32(cosBuffer2, 0.5, audioBufferR_EX, 256);
  //arm_scale_f32(sinBuffer2, 0.2, audioBufferL_EX, 256);
  //arm_scale_f32(cosBuffer2, 0.2, audioBufferR_EX, 256);
  // scaled to give 1W output on 40m when CWPowerCalibrationFactor = 1.0
  // output pwr measured with AD3 (Exp dB ave weight 100 for 500 samples) on -30dB tap of 20W dummy load
  //arm_scale_f32(sinBuffer2, 0.03385 / CWPowerCalibrationFactor[1], audioBufferL_EX, 256);
  //arm_scale_f32(cosBuffer2, 0.03385 / CWPowerCalibrationFactor[1], audioBufferR_EX, 256);
  //arm_scale_f32(sinBuffer2, 0.02, audioBufferL_EX, 256);
  //arm_scale_f32(cosBuffer2, 0.02, audioBufferR_EX, 256);
  //arm_scale_f32(sinBuffer2, 0.05368 / CWPowerCalibrationFactor[1], audioBufferL_EX, 256);
  //arm_scale_f32(cosBuffer2, 0.05368 / CWPowerCalibrationFactor[1], audioBufferR_EX, 256);

  /**********************************************************************************
      Additional scaling, if nesessary to compensate for down-stream gain variations
   **********************************************************************************/
/*
  // adjust IQ signal amplitude and phase
  if(bands[currentBand].demod == DEMOD_LSB) {
    arm_scale_f32(audioBufferL_EX, IQXAmpCorrectionFactor[currentBand], audioBufferL_EX, 256);
    IQPhaseCorrection(audioBufferL_EX, audioBufferR_EX, IQXPhaseCorrectionFactor[currentBand], 256);
  } else if(bands[currentBand].demod == DEMOD_USB) {
    arm_scale_f32(audioBufferL_EX, -IQXAmpCorrectionFactor[currentBand], audioBufferL_EX, 256);
    IQPhaseCorrection(audioBufferL_EX, audioBufferR_EX, IQXPhaseCorrectionFactor[currentBand] * 2.0, 256);
  }
*/
  // ramp signal if requested
  if(ramp) {
    // adjust start or end 10 ms block for a variable time and a 5 ms raised cosine ramp
    // (see https://www.w8ji.com/keyclicks.htm for good discussion on shaping CW signals).
    for(int i = 0; i < 256; i++) {
      if(state == ON) {
        // signal turning on
        int begin = (int)(timeAdjust * 25.5);
        //int end = (int)((5 + timeAdjust) * 25.5);
        int end;

        if(begin > 128) begin = 128;
        end = begin + 128;

        if(i < begin) {
          fac = 0.0;
        } else {
          if(i < end) {
            fac = cwRampUp[i - begin];
          } else {
            fac = 1.0;
          }
        }
      } else {
        // signal turning off
        int end = (int)((10.0 - timeAdjust) * 25.5);
        int begin;

        if(end < 128) end = 128;
        begin = end - 128;

        if(i < begin) {
          fac = 1.0;
        } else {
          if(i < end) {
            fac = cwRampDown[i - begin];
          } else {
            fac = 0.0;
          }
        }
      }
      audioBufferL_EX[i] *= fac;
      audioBufferR_EX[i] *= fac;
    }
  } else if(state == OFF) {
    // signal off, scale to 0
    arm_scale_f32(audioBufferL_EX, 0.0, audioBufferL_EX, 256);
    arm_scale_f32(audioBufferR_EX, 0.0, audioBufferR_EX, 256);
  }

  /**********************************************************************************
    Interpolate (upsample the data streams by 8X to create the 192 kHz sample rate for output
    Requires a LPF FIR 48 tap 10KHz and 8KHz
    **********************************************************************************/

  // interpolation I channel by 2 to 48kHz
  arm_fir_interpolate_f32(&FIR_int1_EX_I, audioBufferL_EX, audioBufferTemp, 256);

  // interpolation I channel by 4 to 192 kHz
  arm_fir_interpolate_f32(&FIR_int2_EX_I, audioBufferTemp, audioBufferL_EX, 512);

  // interpolate 2x and 4x again with Q channel
  arm_fir_interpolate_f32(&FIR_int1_EX_Q, audioBufferR_EX, audioBufferTemp, 256);
  arm_fir_interpolate_f32(&FIR_int2_EX_Q, audioBufferTemp, audioBufferR_EX, 512);

  // scale to compensate for losses in interpolation and output pwr
  if(pwrScale) {
    cwPwr = (6.3749 * pow(tp, 5.0) - 154.46 * pow(tp, 4.0) + 1437.3 * pow(tp, 3.0) - 6384.5 * pow(tp, 2.0) + 17189.0 * tp + 962.75) / 100000.0 * CWPowerCalibrationFactor[currentBand];
  } else {
    //cwPwr = CWPowerEqnCalFactor[currentBand];
    cwPwr = 1.0;
  }
  arm_scale_f32(audioBufferL_EX, cwPwr, audioBufferL_EX, 2048);
  arm_scale_f32(audioBufferR_EX, cwPwr, audioBufferR_EX, 2048);

  /**********************************************************************************
    CONVERT TO INTEGER AND PLAY AUDIO
  **********************************************************************************/

  q15_t q15_buffer_LTemp[2048];
  q15_t q15_buffer_RTemp[2048];

  arm_float_to_q15(audioBufferL_EX, q15_buffer_LTemp, 2048);
  arm_float_to_q15(audioBufferR_EX, q15_buffer_RTemp, 2048);

  // reset CW signal timing if we've started a new signal
  // *** TODO: this will have problems if we don't ramp ***
  if(state == ON && ramp) {
    cwAtomTimer = 0;
  }

  // we'll get discountinuities without this
  // *** TODO: set a default for this and return to that upon any change ***
  Q_out_L_Ex.setBehaviour(AudioPlayQueue::ORIGINAL);
  Q_out_R_Ex.setBehaviour(AudioPlayQueue::ORIGINAL);
  Q_out_L_Ex.play(q15_buffer_LTemp, 2048);
  Q_out_R_Ex.play(q15_buffer_RTemp, 2048);
  Q_out_L_Ex.setBehaviour(AudioPlayQueue::NON_STALLING);
  Q_out_R_Ex.setBehaviour(AudioPlayQueue::NON_STALLING);

  // play sidetone
  // *** TODO: this needs scaled ***
  //Q_out_L.play(q15_buffer_LTemp, 2048);

  if(state == ON && ramp) {
    while(cwAtomTimer < timeAdjust) {
      ;
    }

    // reset cwAtomTimer here at end of on ramp
    // to get most accurate signal timing from CreateCWSignal
    cwAtomTimer = 0;
  }

  if(pause) {
    // CW_ExciterIQData produces ~10ms of data
    // pause while signal plays to prevent needless churn
    CWPause(10);
  }
}

/*****
  Purpose: Create I and Q signal of given length for CW
           Signal consists of a shaped starting and ending blocks and enough 10 ms blocks
           to create a signal of specified length.

  Parameter list:
    unsigned long signalLength
*****/
void CreateCWSignal(unsigned long signalLength) {
  // # of full 10ms blocks (less initial on and final off 10 ms blocks) required to create signal
  int blocks = (signalLength - 20) / 10;
  float timeAdjust = 2.5 * (signalLength / transmitDitLength); // required time adjustment for signal length

  // maintain at least a 5 ms ramp
  // *** this could cause timing to differ slightly from signalLength ***
  if(timeAdjust > 5.0) {
    timeAdjust = 2.5;
    blocks -= 1;
  }

  // queue blocks required for signalLength
  // we won't pause for each call, but pause for the
  // overall signal length after queueing these
  // ramp up
  CW_ExciterIQData(ON, true, false, timeAdjust);

  // body
  for(int i = 0; i < blocks; i++) {
    CW_ExciterIQData(ON, false, false);
  }

  // ramp down
  CW_ExciterIQData(OFF, true, false, timeAdjust);

  // pause while signal plays
  // cwAtomTimer is reset at end of CW_ExciterIQData ramp up
  // for most accurate timing
  while(cwAtomTimer < signalLength) {
    ;
  }
}

void CWTransmit() {
  int valPin;
  int oldVal = HIGH;
  unsigned long cwTransmitTimer;

  // turn on TX relay and initialize CW signal timer
  digitalWrite(RXTX, HIGH); // turn on TX relay
  cwTransmitTimer = millis();

  // start generating CW signal
  while(millis() - cwTransmitTimer <= cwTransmitDelay) {
    valPin = digitalRead(paddleDit);

    // start CW transmit, CW signal timer is on
    switch(valPin) {
      case LOW:
        cwTransmitTimer = millis();
        if(oldVal == HIGH) {
          // begin ramp up
          CW_ExciterIQData(ON, true);
        } else {
          // continue signal
          CW_ExciterIQData();
        }
        break;

      case HIGH:
        if(oldVal == LOW) {
          // begin ramp down
          CW_ExciterIQData(OFF, true);

          // reset CW signal timer
          cwTransmitTimer = millis();
        } else {
          // continue signal
          CW_ExciterIQData(OFF);
        }
        break;

      default:
        break;
    }

    oldVal = valPin;
  }

  digitalWrite(RXTX, LOW);

  // delay a bit to allow play buffer to empty, otherwise
  // the remaining buffer will be played next time it's connected
  CWPause(50);
}
