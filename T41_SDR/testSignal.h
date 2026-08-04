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

/*
  AudioSignal
    Creates sine wave of set frequency, phase and amplitude

  *** the audio library works with 16-bit integers which limits signal resolution ***

  Amplitude   Signal Strength     Position**
      n            dBm               dB
  0.1077217        -30
  0.0270585        -43 (S9+30db)
  0.0085565        -53 (S9+20db)     60***
  0.0008890        -73 (S9)          50
  0.0004456        -79 (S8)
  0.000062943      -97 (S5)          25
  1/65536*         -105              15

  *   minimum signal level
  **  above bottom of 10dB noise floor (~-120dBm)
  *** top of 10dB spectrum

  *** AudioNoInterrupts() should be used with multiple signals to guarantee all new settings take effect at the same time ***
*/
class AudioSignal : public AudioStream {
public:
	AudioSignal() : AudioStream(0, NULL), magnitude(16384) {}

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
    amplitude(0.000889);
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

  AudioSignal& GetI() { return I;}
  AudioSignal& GetQ() { return Q;}

protected:
	//uint32_t phase_accumulator;
	//uint32_t phase_increment;
	//int32_t magnitude;

private:
  AudioSignal I, Q;
};
