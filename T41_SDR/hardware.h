/*

This software accepts hardware specific driver functions listed in the Forwards
section for a standard T41 operation. If the hardware doesn't impliment a given
functionality, the function body can be empty.

Hardware specific functions: *** TODO: complete summary ***

*/

//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

/*

The following defines must be defined regardless of hardware version:

*** TODO: complete summary ***

*/

//-------------------------------------------------------------------------------------------------------------
// Forwards
//-------------------------------------------------------------------------------------------------------------

/*

The following routines are referenced by the common code but their function may vary by specific hardware.  The
functions are defined in hardware specific modules and can be empty allowing the T41 core code, with the selected
hardware version, to compile and run without a given feature. To create a new hardware version, impilement the
desired features from the functions below and the core code will call the routines at the appropriate time.

The InitDisplay function is required at a minimum for an operating display.

*/

// General
float CalcSignalStrength();

// Button.cpp
int ReadSelectedPushButton();
void PreChangeBandHardware();
void PostChangeBandHardware();

// Encoder.cpp
int ReadTuneEncoder();

// Process.cpp
void RemoveDCBias();

// T41_SDR.ino
void InitHardware(int sampleRate);
void SoftResetHardware();
void ConfigRadioStateHardware();
void HardwareLoopStart();
