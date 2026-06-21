
#include <utility/imxrt_hw.h>          // for setting I2S freq

#include "SDT.h"

#ifdef USE_MIC_COMPRESSION
#include <OpenAudio_ArduinoLibrary.h>  // https://github.com/chipaudette/OpenAudio_ArduinoLibrary
#endif

#include "AudioConfig.h"
#include "USBManager.h"

/**************************************************************
T41 audio chain:
  Software supports both RX/TX and RX only versions. RX support is always assumed. But because v11/v12 hardware
  versions do not communicate with their ADC/DAC chips, it isn't possible to directly determin from the main board
  if TX is supported.  To accomodate this, AudioSetup accepts a boolean indicating if TX is supported.

  *** TODO: for v12 determine if other I2C chips can be used to automatically determine if TX is possible ***

  v11/v12 radios:
    audioControl_1 control object associated w/ audio adapter (set to default I2C address 0x0A address, low)
    audioControl_2 control object associated w/ PCM1808 ADC.  Control object set to I2C address 0x2A address, high,
    but there is no actual I2C communication with the PCM1808. Chip is hardwired for I2S operation (as is PCM5102).
    audioControl_2 is legacy, dating back to early versions of the T41 software, but it serves no purpose in the
    v11/v12 radios and can be removed. I've left it in for now as it's used w/ vPS.

    Receive path:
      All:
        PCM1808 ADC -> i2s_quadIn (ch 3&4) -> Q_in_L/R -> DSP - > Q_out_L -> i2s_quadOut (ch 3) -> PCM5102 DAC
      To WSJT-X:
        Q_out_L -> wsjtAmp -> wsjtOut -> (audio + CAT on Serial over USB) -> WSJT-X

    Transmit path:
      SSB:
        Audio adapter mic -> i2s_quadIn (ch 1&2) -> Q_in_L/R_Ex -> DSP - > Q_out_L/R_Ex -> i2s_quadOut (ch 1&2) -> Audio adapter line out -> Exciter
      CW:
        CWTransmit -> DSP - > Q_out_L/R_Ex -> i2s_quadOut (ch 1&2) -> Audio adapter line out -> Exciter
      WSJT-X:
        WSJT-X -> (audio + CAT on Serial over USB) -> wsjtIn -> DSP - > Q_out_L/R_Ex -> i2s_quadOut (ch 1&2) -> Audio adapter line out -> Exciter

      Sidetone: Q_out_L -> i2s_quadOut (ch 3) -> PCM5102 DAC



  vPS and hardware w/ two audio adapter boards:
    audioControl_1 control object on low address associated w/ audio adapter #1 (set to default I2C address 0x0A address, low)
    audioControl_2 control object on high address associated w/ audio adapter #2 (set to I2C address 0x2A address, high)

    Receive path:
      All:
        IQ signals -> Audio adapter #2 line in -> i2s_quadIn (ch 3&4) -> Q_in_L/R -> DSP - > Q_out_L -> i2s_quadOut (ch 3) -> Audio adapter #2 headphone
      To WSJT-X:
        Q_out_L -> wsjtAmp -> wsjtOut -> (audio + CAT on Serial over USB) -> WSJT-X

    Transmit path:
      SSB:
        Audio adapter #1 mic -> i2s_quadIn (ch 1&2) -> Q_in_L/R_Ex -> DSP - > Q_out_L/R_Ex -> i2s_quadOut (ch 1&2) -> Audio adapter #1 line out
      CW:
        CWTransmit -> DSP - > Q_out_L/R_Ex -> i2s_quadOut (ch 1&2) -> Audio adapter #1 line out
      WSJT-X:
        WSJT-X -> (audio + CAT on Serial over USB) -> wsjtIn -> DSP - > Q_out_L/R_Ex -> i2s_quadOut (ch 1&2) -> Audio adapter #1 line out

      Sidetone: Q_out_L -> i2s_quadOut (ch 3) -> Audio adapter #2 headphone



  vAP (remote unit has single audio adapter circuit):
    audioControl_1 control object on low address associated w/ audio adapter circuit #1 (set to default I2C address 0x0A address, low)

    Receive path:
      T41 -> aStream -> Q_in_L/R -> DSP - > Q_out_L -> i2s_quadOut (ch 1) -> Audio adapter #1 headphone

    Transmit path (*** experimental ***):
      SSB:
        to come
      CW:
        to come
      WSJT-X:
        WSJT-X -> (audio + CAT on Serial over USB) -> wsjtIn -> DSP - > Q_out_L/R_Ex -> aStream -> T41



  Remote unit audio connection details:
    Receive path:
      T41 i2s_quadIn -> aStream -> USB or Ethernet cable -> aStream -> Remote Q_in_L/R

    Transmit path (*** experimental ***):
      Remote Q_out_L/R_Ex -> aStream -> USB or Ethernet cable -> aStream -> T41 Q_out_L/R_Ex



  Hardware w/ one audio adapter board (*** Receive only: LOCAL_AUDIO_DATA must be defined ***):
  *** I don't have a dedicated hardware version set up for this radio role ***
    audioControl_1 control object on low address

    Receive path:
      IQ signals -> Audio adapter line in -> i2s_quadIn (ch 3&4) -> Q_in_L/R -> DSP - > Q_out_L/R -> i2s_quadOut (ch 3) -> Audio adapter headphone (or line out)

***************************************************************/

/*
See https://www.reddit.com/r/T41_EP/comments/1jnkhud/restructuring_the_t41_audio_chain/
for a discussion on reconfiguring the T41 audio chain.

The use of mixers to control audio chain flow is inefficient:

  Mixer processor loading and memory usage:
    * each connected mixer adds between 50-250 usec to processor load each loop
    * each connected mixer consumes about 77 bytes
    * removing original v49.2k mixers and their patchcords speeds up main loop by about 2ms

  Memory Usage on Teensy 4.1:
    w/ 20 additional mixers and connections
    FLASH: code:233396, data:192036, headers:8736   free for files:7692296
    RAM1: variables:264416, code:184264, padding:12344   free for local variables:63264
    RAM2: variables:377024  free for malloc/new:147264

    w/o 20 additional mixers and connections
    FLASH: code:232548, data:192036, headers:8560   free for files:7693320
    RAM1: variables:262880, code:183416, padding:13192   free for local variables:64800
    RAM2: variables:377024  free for malloc/new:147264

    w/o all T41 mixers and their patchcords
    FLASH: code:227884, data:191632, headers:8508   free for files:7698440
    RAM1: variables:261184, code:179000, padding:17608   free for local variables:66496
    RAM2: variables:377024  free for malloc/new:147264
*/

//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

extern int currentMicThreshold;

static bool supportsTX = false;

#ifdef AUDIO_STATS
elapsedMicros usecAudio;
#endif

//AudioControlSGTL5000_Extended audioControl_1; // controller for the Teensy Audio Board microphone https://www.janbob.com/electron/OpenAudio_Design_Tool/index.html?info=AudioControlSGTL5000
// https://www.pjrc.com/teensy/gui/?info=AudioControlSGTL5000
AudioControlSGTL5000 audioControl_1; // controller for the Teensy Audio Board microphone
AudioControlSGTL5000 audioControl_2; // control object PCM1808 ADC (doesn't actually control ADC) https://www.pjrc.com/teensy/gui/?info=AudioControlSGTL5000

/*
Note: When adding new audio objects, observe: https://www.pjrc.com/teensy/td_libs_AudioConnection.html

Audio objects should be created in the order data is processed, inputs, playback and synthesis, then effects, filters, mixers, and lastly outputs.

Connections are most efficient when made from an earlier object (in the order they are created) to a later one. Connections from a later object
back to an earlier object can be made, but they add a 1-block delay and consume more memory to implement that delay.
*/

// Audio inputs
// I2S quad input: ch 1&2 on pin 8, ch 3&4 on pin 6
// See https://www.pjrc.com/teensy/gui/?info=AudioInputI2SQuad
AudioInputI2SQuad i2s_quadIn;

// Microphone input (pin 8)
// *** TODO: in orginal software compressor objects are always active
//           restructure to add to chain only when active or examine
//           using the SGTL5000 to preprocess the microphone output ***
#ifdef USE_MIC_COMPRESSION
AudioConvert_I16toF32 int2Float1, int2Float2;  // https://www.janbob.com/electron/OpenAudio_Design_Tool/index.html?info=AudioConvert_I16toF32
AudioEffectCompressor_F32 comp1, comp2;        // https://www.janbob.com/electron/OpenAudio_Design_Tool/index.html?info=AudioEffectCompressor_F32
AudioConvert_F32toI16 float2Int1, float2Int2;  // https://www.janbob.com/electron/OpenAudio_Design_Tool/index.html?info=AudioConvert_F32toI16
#endif

#if T41_WSJT_CAT_AUDIO
AudioInputUSB wsjtIn;
#endif

AudioRecordQueue Q_in_L_Ex;
//AudioRecordQueue Q_in_R_Ex; // *** TODO: this will be used in calibration routines, but not needed for microphone ***

// Remote Audio - Remote IQ data stream:
// The T41 IQ data stream is transfered to a remote unit over USB Host/Ethernet. The remote
// unit receives the data on USB serial/Ethernet. The specific objects are declared below
// based on the mode selected in the hardware config file, hardwareConfig.h, for each unit.
#if RADIO_ROLE == 7
AudioOutputHostSerial iqStreamUSB{USBManager::getHost()};
AudioOutputEthernet iqStreamEthernet;
#elif RADIO_ROLE == 4
AudioInputEthernet iqStreamEthernet;
#elif RADIO_ROLE == 6
AudioInputSerial1 iqStreamUSB;
AudioInputEthernet iqStreamEthernet;
#endif

// default to a Ethernet connection
ConnectBase* cbStream = &iqStreamEthernet;
AudioStream* aStream = &iqStreamEthernet;

// Receive I/Q input (pin 6)
AudioRecordQueue Q_in_L; // https://www.pjrc.com/teensy/gui/?info=AudioRecordQueue
AudioRecordQueue Q_in_R;

// Audio outputs
// I2S quad output: ch 1&2 on pin 7, ch 3&4 on pin 32
// See https://www.pjrc.com/teensy/gui/?info=AudioOutputI2SQuad
AudioOutputI2SQuad i2s_quadOut;

// Exciter I/Q (pin 7)
AudioPlayQueue Q_out_L_Ex; // https://www.pjrc.com/teensy/gui/?info=AudioPlayQueue
AudioPlayQueue Q_out_R_Ex;

// Receiver audio and Sidetone (pin 32)
AudioPlayQueue Q_out_L;

// audio connections are created empty
// source/destination set in AudioSetup
// see audio connection guidelines at: https://www.pjrc.com/teensy/td_libs_AudioConnection.html
AudioConnection pc_Q_in_L, pc_Q_in_R, pc_Q_in_L_Ex, pc_Q_out_L, pc_Q_out_L_Ex, pc_Q_out_R_Ex, pc_IQ_L, pc_IQ_R;

// currently USB Audio only used with WSJT-X FT8
// *** TODO: put these in the proper place for setup ***
#if T41_WSJT_CAT_AUDIO
// *** WSJT-X recommends a signal strength of 30db with signal with only noise ***
// some amplification needed to give reasonable PC input volume setting
AudioAmplifier wsjtAmp;
AudioOutputUSB wsjtOut;
AudioConnection pc_wsjtAmp(Q_out_L, wsjtAmp);
AudioConnection pc_usb1(wsjtAmp, 0, wsjtOut, 0);
//AudioConnection pc_usb1(Q_out_L, 0, wsjtOut, 0);

AudioConnection pc_usb2(wsjtIn, Q_in_L_Ex);
#endif

#ifdef USE_MIC_COMPRESSION
// *** TODO: this hasn't been implimented ***
AudioConnection patchCord1(i2s_quadIn, 0, int2Float1, 0);
AudioConnection patchCord2(i2s_quadIn, 1, int2Float2, 0);
AudioConnection_F32 patchCord3(int2Float1, 0, comp1, 0);
AudioConnection_F32 patchCord4(int2Float2, 0, comp2, 0);
AudioConnection_F32 patchCord5(comp1, 0, float2Int1, 0);
AudioConnection_F32 patchCord6(comp2, 0, float2Int2, 0);
AudioConnection patchCord7(float2Int1, 0, Q_in_L_Ex, 0);
AudioConnection patchCord8(float2Int2, 0, Q_in_R_Ex, 0);
#endif

// *** TODO: consider adding a volume control ***
/*
AudioAmplifier outputAmp; // gain of 0 or 1 handled efficiently. https://www.pjrc.com/teensy/gui/?info=AudioAmplifier
AudioConnection pc_Q_out_L(Q_out_L, 0, outputAmp, 0);
AudioConnection pc_OutputAmp(outputAmp, 0, i2s_quadOut, 2);
*/

//-------------------------------------------------------------------------------------------------------------
// Code
//-------------------------------------------------------------------------------------------------------------

/*****
  Purpose: To set the I2S frequency

  Parameter list:
    int freq        the frequency to set

  Return value:
    int             the frequency or 0 if too large

  Tested I2S sample rates
    8000, // SAMPLE_RATE_8K    // not OK
   11025, // SAMPLE_RATE_11K   // not OK
   16000, // SAMPLE_RATE_16K   // OK
   22050, // SAMPLE_RATE_22K   // OK
   32000, // SAMPLE_RATE_32K   // OK, on
   44100, // SAMPLE_RATE_44K   // OK
   48000, // SAMPLE_RATE_48K   // OK
   50223, // SAMPLE_RATE_50K   // NOT OK
   88200, // SAMPLE_RATE_88K   // OK
   96000, // SAMPLE_RATE_96K   // OK
  100000, // SAMPLE_RATE_100K  // NOT OK
  100466, // SAMPLE_RATE_101K  // NOT OK
  176400, // SAMPLE_RATE_176K  // OK
  192000, // SAMPLE_RATE_192K  // OK
  234375, // SAMPLE_RATE_234K  // NOT OK
  256000, // SAMPLE_RATE_256K  // NOT OK
  281000, // SAMPLE_RATE_281K  // NOT OK
  352800  // SAMPLE_RATE_353K  // NOT OK
*****/
FLASHMEM int SetI2SFreq(int freq) {
  int n1;
  int n2;
  int c0;
  int c2;
  int c1;
  double C;

  // PLL between 27*24 = 648MHz und 54*24=1296MHz
  // Fudge to handle 8kHz - El Supremo
  if(freq > 8000) {
    n1 = 4;  //SAI prescaler 4 => (n1*n2) = multiple of 4
  } else {
    n1 = 8;
  }
  n2 = 1 + (24000000 * 27) / (freq * 256 * n1);
  if(n2 > 63) {
    char msg[50];

    sprintf(msg, "ERROR: n2 exceeds 63 - %d\n", n2);

    // n2 must fit into a 6-bit field
    Debug(msg);
    return 0;
  }
  C = ((double)freq * 256 * n1 * n2) / 24000000;
  c0 = C;
  c2 = 10000;
  c1 = C * c2 - (c0 * c2);
  set_audioClock(c0, c1, c2, true);
  CCM_CS1CDR = (CCM_CS1CDR & ~(CCM_CS1CDR_SAI1_CLK_PRED_MASK | CCM_CS1CDR_SAI1_CLK_PODF_MASK))
               | CCM_CS1CDR_SAI1_CLK_PRED(n1 - 1)   // &0x07
               | CCM_CS1CDR_SAI1_CLK_PODF(n2 - 1);  // &0x3f

  CCM_CS2CDR = (CCM_CS2CDR & ~(CCM_CS2CDR_SAI2_CLK_PRED_MASK | CCM_CS2CDR_SAI2_CLK_PODF_MASK))
               | CCM_CS2CDR_SAI2_CLK_PRED(n1 - 1)   // &0x07
               | CCM_CS2CDR_SAI2_CLK_PODF(n2 - 1);  // &0x3f)
  return freq;
}

void SetupRemoteIQStream(ConnectMode connectMode) {
  if(t41.RadioRole == 0) return;

  if(cbStream) cbStream->end(); // already done by disconnect()

  if(connectMode == CONNECT_ETHERNET) {
    aStream = &iqStreamEthernet;
    cbStream = &iqStreamEthernet;
#if RADIO_ROLE == 7 || RADIO_ROLE == 6
  } else {
    aStream = &iqStreamUSB;
    cbStream = &iqStreamUSB;
#endif
  }


  if(t41.RadioRole == 7) {
    // T41
    pc_IQ_L.disconnect();
    pc_IQ_R.disconnect();
    pc_IQ_L.connect(i2s_quadIn, 2, *aStream, 0);
    pc_IQ_R.connect(i2s_quadIn, 3, *aStream, 1);
  } else if((t41.RadioRole == 4) || (t41.RadioRole == 6)) {
    // remote
    pc_Q_in_L.disconnect();
    pc_Q_in_R.disconnect();
    pc_Q_in_L.connect(*aStream, 0, Q_in_L, 0);
    pc_Q_in_R.connect(*aStream, 1, Q_in_R, 0);
  }

  cbStream->begin();
}

/*****
  Set up audio objects

  Supported hardware configurations:
    - Single audio adapter for TX and hardwired receive chain ADC/DAC (v11/v12)
    - Dual audio adapters for TX and RX
    - Single audio adapter for RX

  Audio adapter assumed at I2C address 0x0A if TX is supported.  Second audio
  adapter, if present, is assumed at I2C address 0x2A.  If TX is not supported,
  audio adapter assumed at I2C address 0x0A for RX.

  w/ I2C address 0x0A: I2S input on Teensy pin 8, I2S output on Teensy pin 7
  w/ I2C address 0x2A: I2S input on Teensy pin 6, I2S output on Teensy pin 32

  *** TODO: examine TX and RX on a single audio adapter ***

  *** see audio connection guidelines at: https://www.pjrc.com/teensy/td_libs_AudioConnection.html ***
  Connections are most efficient when made from an earlier object (in the order they are created) to a later one.
  Connections from a later object back to an earlier object can be made, but they add a 1-block delay and consume
  more memory to implement that delay.

*****/
FLASHMEM void AudioSetup(int sampleRate, bool _supportsTX /* = true */) {
  supportsTX = _supportsTX;

  // set I2S freq to sample rate
  SetI2SFreq(sampleRate);

  // allocate audio library memory
  // about 26k increase in DMAMEM for each 100 block increase in audio memory
  //AudioMemory(100);
  AudioMemory(MAX_AUDIO_BLOCKS);
  //AudioMemory(500);
  //AudioMemory(1000); // about 130k increase in DMAMEM over 500

  // setup control object for the SGTL5000 at address 0x0A
  // this will control the SGTL5000 for TX or RX depending on supportsTX
  // the SGTL5000 is assumed, though a minimal hardware setup without it should still work
  audioControl_1.setAddress(LOW);
  audioControl_1.enable();

  if(supportsTX) {
    // setup input from audio adapter microphone for TX
    audioControl_1.inputSelect(AUDIO_INPUT_MIC);
    //audioControl_1.micGain(20);
    audioControl_1.micGain(10);
    audioControl_1.lineInLevel(0);
    //audioControl_1.lineOutLevel(20);
    audioControl_1.lineOutLevel(13);
    audioControl_1.adcHighPassFilterDisable();  //reduces noise.  https://forum.pjrc.com/threads/27215-24-bit-audio-boards?p=78831&viewfull=1#post78831

    // configure the SGTL5000 control object at address 0x0A for input from the Main board ADC
    // this is a PCM1808 not an SGTL5000 so any I2C related configuration functions aren't usable
    audioControl_2.setAddress(HIGH); // Teensy pin 6
    audioControl_2.enable();
    audioControl_2.inputSelect(AUDIO_INPUT_LINEIN);

    // set headphone audio out volume
    // *** the following doesn't change the line out level ***
    // *** legacy code sets the volume with the control object, but this should fail
    // on v11/12 as there isn't an actual chip at I2C address 0x2A ***
    // *** TODO: check that this returns false on v11/v12 ***
    // with a second audio adapter (as on my modified vPS this gives a reasonable sound level at volume 30)
    audioControl_2.volume(0.5);

    // establish audio connections
    // input from microphone on I2S channel 1 (pin 8)
    pc_Q_in_L_Ex.connect(i2s_quadIn, 0, Q_in_L_Ex, 0);
    //pc_Q_in_R_Ex.connect(i2s_quadIn, 1, Q_in_R_Ex, 0);

    // output to exciter I/Q on I2S channel 1, 2 (pin 7)
    pc_Q_out_L_Ex.connect(Q_out_L_Ex, 0, i2s_quadOut, 0);
    pc_Q_out_R_Ex.connect(Q_out_R_Ex, 0, i2s_quadOut, 1);

    // RX input on I2S channels 3, 4 (pin 6)
    pc_Q_in_L.connect(i2s_quadIn, 2, Q_in_L, 0);
    pc_Q_in_R.connect(i2s_quadIn, 3, Q_in_R, 0);

    // RX output and sidetone on I2S channel 3(left)
    // I2S on pin 32 (also headphone and line out w/ 2nd audio adapter)
    pc_Q_out_L.connect(Q_out_L, 0, i2s_quadOut, 2);
  } else {
    // RX only
    // setup input from audio adapter line in for RX
    // *** TODO: examine USB audio ***
    audioControl_1.inputSelect(AUDIO_INPUT_LINEIN);

    // set headphone audio out volume
    // *** the following doesn't change the line out level ***
    // this gives a reasonable sound level at volume 30
    audioControl_1.volume(0.5);

    // establish audio connections
    // RX input on I2S channels 1, 2 (pin 8)
#if LOCAL_AUDIO_DATA
    pc_Q_in_L.connect(i2s_quadIn, 0, Q_in_L, 0);
    pc_Q_in_R.connect(i2s_quadIn, 1, Q_in_R, 0);
#endif

    // RX output on I2S channel 1(left)
    // I2S on pin 7 (also headphone and line out)
    pc_Q_out_L.connect(Q_out_L, 0, i2s_quadOut, 0);
  }

  // set behavior of audio play queues
  // *** TODO: examine need for these with regards to audio memory ***
  // enabling these causes unstable CW behavior *** TODO: examine this and provide details ***
  // *** TODO: consider activating these only when needed, like FT8 for Q_out_L

  // Q_out_L can buffer up to 80 blocks. setMaxBuffers can limit this to prevent play queue from buffering to much
  // I haven't found setMaxBuffers solving a high memory use
  //Q_out_L.setMaxBuffers(40);
  //Q_out_L.setBehaviour(AudioPlayQueue::ORIGINAL); // memory buffer for output queues are limited so this can be set without effect if problem is with input queue
  Q_out_L.setBehaviour(AudioPlayQueue::NON_STALLING); // FT8 decoding slow without this *** TODO: examine audio memory issues ***


  // *** TODO: cause discountinuities in calibration tones ***
  //Q_out_L_Ex.setBehaviour(AudioPlayQueue::NON_STALLING);
  //Q_out_R_Ex.setBehaviour(AudioPlayQueue::NON_STALLING);

  // *** TODO: put these in the appropriate places above ***
#ifdef USE_MIC_COMPRESSION
  comp1.setPreGain_dB(-10);
  comp2.setPreGain_dB(-10);
#endif

#if T41_WSJT_CAT_AUDIO
// *** WSJT-X recommends a signal strength of 30db with signal with only noise ***
// adjust amplification to give reasonable PC input volume setting
// 100 gain requires PC volume of 1
// 10 gain requires PC volume of 88
// 15 gain requires PC volume of 70
// 20 gain requires PC volume of 60
// 25 gain requires PC volume of ~30
  wsjtAmp.gain(25);
  pc_usb2.disconnect(); // USB
#endif
  AudioMemoryUsageMaxReset(); // reset max audio mem usage
}

inline void Q_in_Ex_Stop() {
  Q_in_L_Ex.end();
  //Q_in_R_Ex.end();
  pc_Q_in_L_Ex.disconnect();
  //pc_Q_in_R_Ex.disconnect();
  Q_in_L_Ex.clear();
  //Q_in_R_Ex.clear();
}

inline void Q_in_Stop() {
  Q_in_L.end();
  Q_in_R.end();
  pc_Q_in_L.disconnect();
  pc_Q_in_R.disconnect();
  Q_in_L.clear();
  Q_in_R.clear();
}

inline void Q_out_Ex_Stop() {
  pc_Q_out_L_Ex.disconnect();
  pc_Q_out_R_Ex.disconnect();
}

inline void Q_out_Stop() {
  pc_Q_out_L.disconnect();
}

inline void Q_in_Ex_Start() {
  pc_Q_in_L_Ex.connect();
  //pc_Q_in_R_Ex.connect();
  Q_in_L_Ex.begin();
  //Q_in_R_Ex.begin();
}

inline void Q_in_Start() {
  pc_Q_in_L.connect();
  pc_Q_in_R.connect();
  Q_in_L.begin();
  Q_in_R.begin();
}

inline void Q_out_Ex_Start() {
  pc_Q_out_L_Ex.connect();
  pc_Q_out_R_Ex.connect();
}

inline void Q_out_Start() {
  pc_Q_out_L.connect();
}

void ConfigAudioState(int audioState) {
  // stop and clear receive and transmit queues
  Q_in_Stop();
  Q_out_Stop();

  if(supportsTX) {
    Q_in_Ex_Stop();
    Q_out_Ex_Stop();
  }

  // Some modes change AudioPlayQueue objects to NON_STALLING behavior as required for best performance.
  // Return AudioPlayQueue objects to default ORIGINAL behavior (enum behaviour_e {ORIGINAL, NON_STALLING}).
  // *** TODO: determine which modes require NON_STALLING behavior and code below ***
  //Q_out_L.setBehaviour(AudioPlayQueue::ORIGINAL);
  if(supportsTX) {
    Q_out_L_Ex.setBehaviour(AudioPlayQueue::ORIGINAL);
    Q_out_R_Ex.setBehaviour(AudioPlayQueue::ORIGINAL);
  }

  switch(audioState) {
    // *** TODO: need to configure data states for both internal and external FT8
    // *** WSJT-X can still decode with audio sent over USB at 192kHz ***
    // *** TODO: consider passing audio to WSJT-X in internal FT8 mode ***
    case RECEIVE_STATE:
      switch(t41.DemodMode) {
        case DEMOD_FT8:
          //Q_out_Ex_Stop();

          #if T41_WSJT_CAT_AUDIO
            pc_usb2.disconnect(); // USB
            //Q_in_L_Ex.end();
            //Q_in_L_Ex.clear();

            pc_wsjtAmp.connect();
          #endif
          break;

        case DEMOD_FT8_INTERNAL:
          //Q_out_Ex_Stop();
          break;

        case DEMOD_FT8_WAV:
        default:
          break;
      }

      // start receive audio chain
      Q_in_Start();

      // *** TODO: CALIBRATE_RECEIVE_STATE had this off ***
      Q_out_Start();
      break;

    case SSB_TRANSMIT_STATE:
      //digitalWrite(MUTE, HIGH);  // mute audio

      #ifdef USE_MIC_COMPRESSION
      if(t41.Compressor == 1) {
        SetupMicCompressors((float)currentMicThreshold, .1, 2.0);
      } else if(t41.Compressor == 0) {
        SetupMicCompressors(0.0, 0.01, 0.01);
      }
      #endif
      audioControl_1.micGain(10);

      // start transmit audio chain
      Q_in_Ex_Start();
      Q_out_Ex_Start();
      break;

    case CW_TRANSMIT_STRAIGHT_STATE:
    case CW_TRANSMIT_PADDLE_STATE:
    case CW_TRANSMIT_KEYER_STATE:
      // start transmit audio chain and sidetone
      Q_out_Ex_Start();
      Q_out_Start(); // sidetone
      break;

    case DATA_TRANSMIT_STATE:
      switch(t41.DemodMode) {
        case DEMOD_FT8:
          // start USB audio transmit chain
          #if T41_WSJT_CAT_AUDIO
            pc_usb2.connect(); // USB
            Q_in_L_Ex.begin();

            pc_wsjtAmp.disconnect();
            Q_out_Start();
          #endif
          break;

        case DEMOD_FT8_INTERNAL:
          Q_out_Start(); // sidetone
          break;

        case DEMOD_FT8_WAV:
        default:
          break;
      }

      Q_out_Ex_Start();
      break;

    case CALIBRATE_TRANSMIT_STATE:
      // set calibration state
      Q_in_Start();
      //Q_in_Ex_Start(); // *** for v12??? ***
      Q_out_Ex_Start();

      Q_out_Start(); // *** TODO: why doesn't this give audio during v11 cal ***
      break;

    case CALIBRATE_TWOTONE_STATE:
      // set calibration state
      Q_in_Ex_Start();
      Q_out_Ex_Start();

      //pc_Q_in_R_Ex.connect();
      //Q_in_R_Ex.begin();
      break;

    case CALIBRATE_DONE_STATE:
      break;

    default:
      break;
  }
}

#ifdef USE_MIC_COMPRESSION
/*****
  Purpose: Setup Teensy Mic Compressor*****/
FLASHMEM void SetupMicCompressors(float knee_dBFS, float attack_sec, float release_sec) {
  boolean use_HP_filter = true; //enable the software HP filter to get rid of DC?
  float comp_ratio = 5.0;
  float fs_Hz = AUDIO_SAMPLE_RATE;

  comp1.enableHPFilter(use_HP_filter);
  comp2.enableHPFilter(use_HP_filter);
  comp1.setThresh_dBFS(knee_dBFS);
  comp2.setThresh_dBFS(knee_dBFS);
  comp1.setCompressionRatio(comp_ratio);
  comp2.setCompressionRatio(comp_ratio);

  comp1.setAttack_sec(attack_sec, fs_Hz);
  comp2.setAttack_sec(attack_sec, fs_Hz);
  comp1.setRelease_sec(release_sec, fs_Hz);
  comp2.setRelease_sec(release_sec, fs_Hz);
}
#endif

#ifdef AUDIO_STATS

void StartAudioStats() {
  usecAudio = 0;
  AudioMemoryUsageMaxReset();
}

void EndAudioStats() {
  //int tmp = usecAudio;
  //Serial.print("Audio stat: ");
  //Serial.println(usecAudio);
  //Serial.println(tmp);
  //Serial.println(modeSelectInL.processorUsage());
  Serial.println(AudioMemoryUsageMax());
}

#endif
