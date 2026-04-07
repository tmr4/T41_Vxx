
//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

#define MAX_AUDIO_VOLUME        100
#define MIN_AUDIO_VOLUME          0

#define ENCODER_DELAY             100L // Menu options scroll too fast!
#define ENCODER_FACTOR            0.4  // gives 100 Hz change with my encoders

extern bool volumeChangeFlag;
extern bool fineTuneFlag;
extern bool resetTuningFlag;  // Experimental flag for ResetTuning() due to possible timing issues
extern bool getEncoderValueFlag;

extern int posFilterEncoder, lastFilterEncoder;
extern long filter_pos_BW;
extern long last_filter_pos_BW;

extern volatile int menuEncoderMove;
extern volatile long fineTuneEncoderMove;
extern volatile int tuneChange;

extern float adjustVolEncoder;

//-------------------------------------------------------------------------------------------------------------
// Code
//-------------------------------------------------------------------------------------------------------------

void SetBWFilters();
//void EncoderCenterTune();
