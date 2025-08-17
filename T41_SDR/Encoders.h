
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

#ifdef FOURSQRP_FRONTPANEL
#include <Rotary.h>                    // https://github.com/brianlow/Rotary

extern Rotary volumeEncoder;        // (2,  3)
extern Rotary tuneEncoder;          // (16, 17)
extern Rotary menuChangeEncoder;    // (14, 15)
extern Rotary fineTuneEncoder;      // (4,  5);
#else
#ifdef MCP23017_FRONTPANEL
#include "src\FrontPanel.h"

extern Rotary_V12 volumeEncoder;
extern Rotary_V12 menuChangeEncoder;
extern Rotary_V12 tuneEncoder;
extern Rotary_V12 fineTuneEncoder;

#endif
#endif

extern float adjustVolEncoder;
extern int tuneChange;

//-------------------------------------------------------------------------------------------------------------
// Code
//-------------------------------------------------------------------------------------------------------------

void SetBWFilters();
void EncoderCenterTune();
float GetEncoderValueLive(float minValue, float maxValue, float startValue, float increment, char prompt[]);

#ifdef FOURSQRP_FRONTPANEL
void EncodersInit();
void EncoderVolumeISR();
void EncoderFineTuneISR();
void EncoderMenuChangeFilterISR();
#else
#ifdef MCP23017_FRONTPANEL
void EncoderVolume();
void EncoderFineTune();
void EncoderFilter();
#endif
#endif
