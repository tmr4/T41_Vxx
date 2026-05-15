// vMP Mini Platform specific hardware config file for running on Audio Platform

#define VERSION "vMP_dev/v0.02" // up to 14 characters can fit *** can adjust infobox ShowVersion routine to accommodate more if needed ***

#define PROFILER_ACTIVE

#define MASTER_CLK_MULT 4ULL // FOURSQRP QSD frontend requires 4x clock

// *** needs to be true if using center tune encoder on MCP23017
#define READ_CENTERTUNE_ENCODER false // set to false if interrupt driven

// pick one of the following display configurations
#define DISPLAY_LANDSCAPE
//#define DISPLAY_FLIPPED

#define VOLUME_REVERSED false
#define FILTER_REVERSED false
#define MAIN_TUNE_REVERSED false
#define FINE_TUNE_REVERSED false

// uncomment below for USB Host support
#define USB_HOST_SUPPORT

// uncomment below for specific USB Host device support
#define HOST_KEYBOARD_MOUSE_SUPPORT // uses about 44k of stack
//#define HOST_SERIAL_SUPPORT 1 // uses USBSerial_BigBuffer

// Remote control
// set one line below to true to enable remote control
#define CAT_CONTROL_T41_USB_HOST  false  // cable from USB host on this unit to serial on remote
#define CAT_CONTROL_REMOTE       false  // cable from serial on this unit to USB host on remote
#define T41_WSJT_CAT_AUDIO     false  // *** for passing audio back and forth with WSJT-X over USB at 44.1kHz sample rate in FT8 mode ***

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
#define controlSerial Serial // Serial or SerialUSB1 for USB port or usbHostSerial for USB Host port (this unit receives/sends CAT cmds over USB Host)
#define beaconSerial Serial // Serial or SerialUSB2
#define wsjtSerial Serial // Serial or SerialUSB1 or SerialUSB2
