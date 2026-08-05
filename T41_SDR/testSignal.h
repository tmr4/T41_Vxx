#pragma once

/* Audio Library for Teensy 3.X
 * Copyright (c) 2014, Paul Stoffregen, paul@pjrc.com
 *
 * Development of this audio library was funded by PJRC.COM, LLC by sales of
 * Teensy and Audio Adaptor boards.  Please support PJRC's efforts to develop
 * open source software by purchasing Teensy or other PJRC products.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice, development funding notice, and this permission
 * notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

// modified from synth_sine.h

#include <Arduino.h>     // github.com/PaulStoffregen/cores/blob/master/teensy4/Arduino.h
#include <AudioStream.h> // github.com/PaulStoffregen/cores/blob/master/teensy4/AudioStream.h
#include <arm_math.h>    // github.com/PaulStoffregen/cores/blob/master/teensy4/arm_math.h
#include <Audio.h>

#include "Display.h"

/*
  AudioSignal
    Creates sine wave of set frequency, phase and amplitude

  *** The audio library works with 16-bit integers which limits signal resolution.    ***
  *** The T41 works within a signal strength range that is a fraction of what the     ***
  *** audio library can produce. Harmonics increase at these low levels though these  ***
  *** are normally below the 10dB noise floor at about -120dBm. The signal spreads at ***
  *** the noise floor as the amplitude increases. The S9 signal is best for testing.  ***

  Signals useful for T41 testing:

  Amplitude   Signal Strength     Position*  Spread**   setSignalStrength
      n            dBm               dB         Hz          parameter
  0.0270585        -43 (S9+30db)                                30
  0.0085565        -53 (S9+20db)     70***      48              20
  0.00095258       -73 (S9)          50          8               9
  0.0004666        -79 (S8)          45          5               8
  0.000062943      -97 (S5)          25          1               5
  1/65536****      -105 (S3-S4)      15          1               0

  *    above bottom of 10dB noise floor (-120dBm)
  **   at noise floor
  ***  top of 10dB spectrum
  **** minimum signal level

  *** AudioNoInterrupts() should be used with multiple signals to guarantee all new settings take effect at the same time ***
*/
class AudioSignal : public AudioStream {
public:
	AudioSignal() : AudioStream(0, NULL) {
    // set default 1kHz signal
    frequency(1000);
    phase(0);
    amplitude(0.5);
  }

  // set the signal frequency (0 to 96000)
	void frequency(float freq) {
		if(freq < 0.0f) {
      freq = 0.0;
    }	else if(freq > 192000.0 / 2.0f)
    {
      freq = 192000.0 / 2.0f;
    }
		phase_increment = freq * (4294967296.0f / 192000.0); // convert float to unsigned 32-bit int
	}

  // sets the signal phase angle (0 to 360 degrees)
	void phase(float angle) {
		if(angle < 0.0f) {
      angle = 0.0f;
    } else if(angle > 360.0f) {
			angle = angle - 360.0f;
			if(angle >= 360.0f) return;
		}
		phase_accumulator = angle * (float)(4294967296.0 / 360.0);
	}

  // sets the signal amplitude (0 to 1)
  // *** call with n=0 for no signal ***
  // with 0 < n < 1/65536 magnitude = 1, to ensure some signal is produced
	void amplitude(float n) {
    if(n == 0.0f) {
      n = 0;
    } else if(n < 0.0000152587890625f) {
      magnitude = 1; // ensure we have some signal
      return;
    }	else if(n > 1.0f) {
      n = 1.0f;
    }
		magnitude = n * 65536.0f;
	}

  void setSignalStrength(uint8_t s);

	virtual void update(void);

protected:
	uint32_t phase_accumulator; // current point within signal cycle in fractions of a degree expressed as an unsigned 32-bit int
	uint32_t phase_increment; // normalized frequency expressed as an unsigned 32-bit int
	int32_t magnitude; // range from 0 to 65536
};

class AudioQuadratureSignals {
public:
	AudioQuadratureSignals() {
    // default to 1kHz LSB S9 signal
    phase();
    frequency(49000.0);
    amplitude(0.00095258);
  }

	void frequency(float freq) {
    AudioNoInterrupts();
    I.frequency(freq);
    Q.frequency(freq);
    AudioInterrupts();
	}


	void phase() {
    AudioNoInterrupts();
    I.phase(90.0);
    Q.phase(0.0);
    AudioInterrupts();
	}

	void amplitude(float n) {
    AudioNoInterrupts();
    I.amplitude(n);
    Q.amplitude(n);
    AudioInterrupts();
	}

  void setSignalStrength(uint8_t s) {
    AudioNoInterrupts();
    I.setSignalStrength(s);
    Q.setSignalStrength(s);
    AudioInterrupts();
  }

  AudioSignal& GetI() { return I;}
  AudioSignal& GetQ() { return Q;}

private:
  AudioSignal I, Q;
};

// sweep frequency start/end in Hz
// sweep duration in seconds
class AudioQuadratureSweep : public AudioQuadratureSignals {
public:
  AudioQuadratureSweep(float s, float e, float duration) : start(s), end(e) {
    increment = (end - start) / duration / 1000.0;
    freq = start;
    frequency(freq);
  }

  void IncSignalFreq() {
    freq += increment;
    if(freq > end) freq = start;
    frequency(freq);
  }

  void DrawSweepLine(int y) {
    DrawSpectrumLine(y);
  }

protected:
  float start, end, freq, increment;
};
