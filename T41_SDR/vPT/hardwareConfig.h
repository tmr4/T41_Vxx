// vPS specific hardware config file for running on Project System

#define VERSION "vPS_dev.1" // Change this for updates. If you make this longer than 9 characters, brace yourself for surprises

#define PROFILER_ACTIVE

#define MASTER_CLK_MULT 4ULL // FOURSQRP QSD frontend requires 4x clock

// pick one of the following display configurations
//#define DISPLAY_LANDSCAPE
#define DISPLAY_FLIPPED

#define READ_CENTERTUNE_ENCODER false // set to false if interrupt driven

#define USE_BUFFERED_FT8_WAV // buffered wav file used for internal FT8 testing

// *** currently Rotary and Rotary_V12 can't coexist so only ENCODER_1 or ENCODER_MCP be enabled ***
// *** currently these are both controlling the volume encoder so only one should be enabled at a time ***
//#define PROJECTSYSTEM_ENCODER_1   // for testing v11 type encoder w/ project system
//#define PROJECTSYSTEM_ENCODER_2   // for testing v11 type encoder w/ project system
//#define PROJECTSYSTEM_ENCODER_3   // for testing v11 type encoder w/ project system

#define VOLUME_REVERSED false
#define FILTER_REVERSED false
#define MAIN_TUNE_REVERSED false
#define FINE_TUNE_REVERSED false

// uncomment below for USB Host support
#define USB_HOST_SUPPORT

// uncomment below for specific USB Host device support
#define HOST_KEYBOARD_MOUSE_SUPPORT // uses about 44k of stack
#define HOST_SERIAL_SUPPORT
//#define HOST_CAT_CONTROL_SUPPORT // enables CAT control over USB host

// Select one of the noted serial objects according to compile options for enabled services.
// For use with PC apps and connecting to other CAT controlled units over USB serial or the USB host connector.
//   Serial:        compiling with a single serial object with multiple services are enabled. Arduino IDE must be closed to connect to PC apps.
//   SerialUSB1:    compiling with Dual or Triple Serial (need to figure which COM port is associated with each)
//   SerialUSB2:    compiling with Triple Serial with two or more services enabled (need to figure which COM port is associated with each)
//   usbHostSerial: for CAT control with another radio over USB host (not for use with PC)
//
// Notes:
// Set disabled services to Serial. Any messages from these services are sent to Arduino serial monitor.
// *** note: debug messages go out over Serial and will be transmitted if controlSerial
// is set to Serial and the unit is connected to the USB host of another unit ***
#define controlSerial Serial // Serial or SerialUSB1 for USB port or usbHostSerial for USB Host port
#define beaconSerial Serial // Serial or SerialUSB2
#define wsjtSerial Serial // Serial or SerialUSB1 or SerialUSB2
