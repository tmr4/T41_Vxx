
#include <Bounce.h>

#include "..\SDT.h"

#include "FrontPanel.h"

//#include "Calibrate.h"
#include "..\CWProcessing.h"
#include "..\Encoders.h"
#include "..\MenuProc.h"
#include "..\Tune.h"

//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

#ifdef PROJECTSYSTEM_ENCODER_MCP
Rotary_V12 volumeEncoder( VOLUME_REVERSED );
Rotary_V12 tuneEncoder( MAIN_TUNE_REVERSED );
Rotary_V12 menuChangeEncoder( FILTER_REVERSED );
Rotary_V12 fineTuneEncoder( FINE_TUNE_REVERSED );
#endif

#ifdef PROJECTSYSTEM_ENCODER_1
Rotary volumeEncoder = Rotary(VOLUME_ENCODER_A, VOLUME_ENCODER_B);        // ( 2,  3)
Bounce encoderSwitch = Bounce(ENCODER_1_SWITCH, 10);  // 10 ms debounce
#endif
#ifdef PROJECTSYSTEM_ENCODER_2
Rotary menuChangeEncoder = Rotary(FILTER_ENCODER_A, FILTER_ENCODER_B);        // ( 2,  3)
Bounce encoder2Switch = Bounce(ENCODER_2_SWITCH, 10);  // 10 ms debounce
#endif

//-------------------------------------------------------------------------------------------------------------
// Forwards
//-------------------------------------------------------------------------------------------------------------

void ProcessMenuEncoder();

void EncoderVolumeISR();

//-------------------------------------------------------------------------------------------------------------
// Code
//-------------------------------------------------------------------------------------------------------------

// set up encoders
#if defined(PROJECTSYSTEM_ENCODER_1) || defined(PROJECTSYSTEM_ENCODER_2)
void EncodersInit() {
#ifdef PROJECTSYSTEM_ENCODER_1
  volumeEncoder.begin(true);
  attachInterrupt(digitalPinToInterrupt(VOLUME_ENCODER_A), EncoderVolumeISR, CHANGE);
  attachInterrupt(digitalPinToInterrupt(VOLUME_ENCODER_B), EncoderVolumeISR, CHANGE);

  // set up encoder switch debounce
  pinMode(ENCODER_1_SWITCH, INPUT_PULLUP);
#endif
#ifdef PROJECTSYSTEM_ENCODER_2
  pinMode(FILTER_ENCODER_A, INPUT);
  pinMode(FILTER_ENCODER_B, INPUT);

  menuChangeEncoder.begin(true);
  attachInterrupt(digitalPinToInterrupt(FILTER_ENCODER_A), EncoderMenuChangeFilterISR, CHANGE);
  attachInterrupt(digitalPinToInterrupt(FILTER_ENCODER_B), EncoderMenuChangeFilterISR, CHANGE);
  Serial.println("menu encoder init");

  // set up encoder switch debounce
  pinMode(ENCODER_2_SWITCH, INPUT_PULLUP);
#endif
}
#endif

#ifdef PROJECTSYSTEM_ENCODER_1
/*****
  Purpose: Encoder volume control ISR
*****/
// why not FASTRUN
void EncoderVolumeISR() {
  char result = 0;

  result = volumeEncoder.process();  // Read the encoder

  if(result == 0) {  // Nothing read
    return;
  }

  // TODO: check encoder setup as this is opposite T41
  switch(result) {
    case DIR_CW:  // Turned it clockwise, 16
      adjustVolEncoder = -1;
      break;

    case DIR_CCW:  // Turned it counter-clockwise
      adjustVolEncoder = 1;
      break;
  }

  if((calibrateItem >= 1) && (calibrateItem <= 3)) return;

  audioVolume += adjustVolEncoder;
  adjustVolEncoder = 0;

  if(audioVolume > MAX_AUDIO_VOLUME) {
    audioVolume = MAX_AUDIO_VOLUME;
  } else if(audioVolume < MIN_AUDIO_VOLUME) {
    audioVolume = MIN_AUDIO_VOLUME;
  }

  volumeChangeFlag = true; // flag needed for display update
}
#endif

#ifdef PROJECTSYSTEM_ENCODER_2
/*****
  Purpose: Menu/Change/Filter encoder movement ISR
*****/
FASTRUN void EncoderMenuChangeFilterISR() {
  char result;

  result = menuChangeEncoder.process();  // Read the encoder

  if(result == 0) {
    return;
  }

  switch(result) {
    case DIR_CW:  // Turned it clockwise, 16
      menuEncoderMove = 1;
      break;

    case DIR_CCW:  // Turned it counter-clockwise
      menuEncoderMove = -1;
      break;
  }

  ProcessMenuEncoder();
}
#endif

#if defined(PROJECTSYSTEM_ENCODER_1) || defined(PROJECTSYSTEM_ENCODER_2)
void EncoderCenterTune() {
}
#endif

#ifdef PROJECTSYSTEM_ENCODER_MCP
/*****
  Purpose: Set center tune frequency based on
*****/
void EncoderCenterTune() {
  int result;

  result = tuneEncoder.process();  // Read the encoder

  if(result == 0)  // Nothing read
    return;

  if(radioMode == CW_MODE && decoderFlag == ON) {  // No reason to reset if we're not doing decoded CW
    ResetHistograms();
  }

  tuneChange = result;

  // *** TODO: from v12, validate v11 calibration routines
  // center tune used in calibration routines, return to process
  //   - receive calibrate adjusts noise floor
  //   - transmit calibrate adjusts image value
  //   - two tone adjusts tone 1
  if((calibrateItem >= 1) && (calibrateItem <= 3)) return;

  SetCenterTune((long)freqIncrement * tuneChange);
}

/*****
  Purpose: Encoder volume control
*****/
// why not FASTRUN
// TODO: front panel placeholders for now
void EncoderVolume() {
  int result;

  result = volumeEncoder.process();  // Read the encoder


  if(result == 0) {  // Nothing read
    return;
  }

  adjustVolEncoder = result;

  if((calibrateItem >= 1) && (calibrateItem <= 3)) return;

  audioVolume += adjustVolEncoder;
  adjustVolEncoder = 0;

  if(audioVolume > MAX_AUDIO_VOLUME) {
    audioVolume = MAX_AUDIO_VOLUME;
  } else if(audioVolume < MIN_AUDIO_VOLUME) {
    audioVolume = MIN_AUDIO_VOLUME;
  }

  volumeChangeFlag = true; // flag needed for display update
}

/*****
  Purpose: Fine tune control
*****/
// TODO: front panel placeholders for now
void EncoderFineTune() {
  int result;

  result = fineTuneEncoder.process();  // Read the encoder

// *** TODO: we'll go through here many times if fine tune encoder bounces ***
  // *** If fineTuneEncoderMove isn't processed in the meantime,
  //    and result == 0, then fineTuneEncoderMove will be reset to zero ***

  if(result == 0) {                   // Nothing read
    fineTuneEncoderMove = 0L;
    return;
  }

  fineTuneEncoderMove = result;

  // *** TODO: from v12, validate v11 calibration routines
  // fine tune used in calibration routines, return to process
  //   - receive calibrate adjusts In/Out attenuation
  //   - transmit calibrate adjusts In/Out attenuation
  //   - two tone adjusts tone 2
  if((calibrateItem >= 1) && (calibrateItem <= 3)) {
    // TODO: not currently used in v12
    //calNFAdjust -= fineTuneEncoderMove;
    fineTuneEncoderMove = 0;
    return;
  }

  SetFineTune(ftIncrement * fineTuneEncoderMove);

  fineTuneEncoderMove = 0L;
}

/*****
  Purpose: Menu/Change/Filter encoder movement
*****/
// TODO: front panel placeholders for now
void EncoderFilter() {
  int result;

  result = menuChangeEncoder.process();  // Read the encoder

  if(result == 0) {
    return;
  }

  menuEncoderMove = result;

  ProcessMenuEncoder();
}

#endif
