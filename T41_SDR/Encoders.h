
//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

#define MAX_AUDIO_VOLUME        100
#define MIN_AUDIO_VOLUME          0

extern bool resetTuningFlag;
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

void ProcessFilterEncoder();
void ProcessMenuEncoder();
bool ProcessCenterTuneEncoder(bool readEncoder = false);
